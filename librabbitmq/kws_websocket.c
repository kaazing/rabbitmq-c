#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "kws_websocket.h"
#include "kws_websocket_channel.h"
#include "kws_rfc6455util.h"

static const char* HTTP_101_MESSAGE = "HTTP/1.1 101 Web Socket Protocol Handshake";
static const char* UPGRADE_HEADER = "Upgrade: ";
static const char* UPGRADE_VALUE = "websocket";
static const char* CONNECTION_MESSAGE = "Connection: Upgrade";
static const char* WEBSOCKET_PROTOCOL = "Sec-WebSocket-Protocol";
static const char* WEBSOCKET_EXTENSIONS = "Sec-WebSocket-Extensions";
static const char* WEBSOCKET_ACCEPT = "Sec-WebSocket-Accept";

static const int WEBSOCKET_MAX_READ_SIZE = 1024;

int kws_send_data_internal(kws_websocket_channel_t* wschannel, const char* data,  int len, int opcode) {
	char* payload = kws_rfc6455_encode(data, len, opcode, 1);
	int payload_size = kws_rfc6455_calculate_header_size(len, 1) + len;
	int sent;
	if (wschannel->sslconnection) {
		sent = SSL_write(wschannel->sslconnection, payload, payload_size);
	}
	else {
	    sent = send(wschannel->socketref, payload, payload_size, 0);
	}
	return (sent == payload_size ? EXIT_SUCCESS : EXIT_FAILURE);
}

int kws_handle_handshake_failed(kws_websocket* ws) {
	//handshake failed
	ws->readyState = kws_websocket_closed;
	pthread_mutex_lock(&ws->channel->condition_mutex);
	pthread_cond_signal(&ws->channel->condition_cond);
	pthread_mutex_unlock(&ws->channel->condition_mutex);
	return EXIT_FAILURE;

}

void kws_close_socket(kws_websocket_channel_t* wschannel) {
	if (wschannel) {
		if (wschannel->socketref) {
	        close(wschannel->socketref);
	        wschannel->socketref = 0;
		}
		if (wschannel->sslconnection)
		{
			SSL_shutdown(wschannel->sslconnection);
			SSL_free(wschannel->sslconnection);
			wschannel->sslconnection = NULL;
		}
		if (wschannel->sslcontext) {
			SSL_CTX_free(wschannel->sslcontext);
			wschannel->sslcontext = NULL;
		}
	}
}
int kws_process_http_response(kws_websocket* ws, int len) {
	kws_websocket_channel_t* wschannel = ws->channel;
	// append data to headerbuffer
	strncat(wschannel->decodebuffer, wschannel->readbuffer, len);

	//fast failure report: response must start with "HTTP/1.1"
	if (strncmp(HTTP_101_MESSAGE, wschannel->decodebuffer, len > strlen(HTTP_101_MESSAGE) ? strlen(HTTP_101_MESSAGE) : len) != 0 ) {
		  //websocket handshake failed
		goto handshake_failed;
	}

	//search for end of HTTP headers
	char *end_of_header, *start, *end;
	end_of_header = strstr(wschannel->decodebuffer, "\r\n\r\n");
	if (end_of_header == NULL) {
		return EXIT_SUCCESS;
	}
	//printf("RECEIVED: %s\n", ws->headerbuffer);

	start = wschannel->decodebuffer;
	char header[256];
	char* protocol = NULL;
	char* extension = NULL;
	int upgradReceived = 1, webSocketKeyReceived = 1, connectionReceived = 1;

	while (1) {
		end = strstr(start, "\r\n");
		if (end > end_of_header) {
			break; //finish parse http headers
		}
	    strncpy(header, start, end - start);
	    header[end - start] = '\0';
		if (strstr(header, WEBSOCKET_PROTOCOL) == header) {
			protocol = strstr(header,":") + 2;
		}
		else if (strstr(header, WEBSOCKET_EXTENSIONS) == header) {
			extension = strstr(header,":") + 2;
		}
		else if (strstr(header, UPGRADE_HEADER) == header) {
			upgradReceived = strcasecmp(UPGRADE_VALUE, strstr(header, ":") + 2);
		}
		else if (strcasecmp(header, CONNECTION_MESSAGE) == 0) {
			connectionReceived = 0;
		}
		else if (strstr(header, WEBSOCKET_ACCEPT) == header) {
			webSocketKeyReceived = strcasecmp(strstr(header, ":") + 2, kws_rfc6455_accept_key_hash(wschannel->websocketkey));
		}

	    start = end + 2;
	  }
	  if (upgradReceived + connectionReceived + webSocketKeyReceived == 0) {
		  wschannel->ws_state = kws_decode_header;
		  wschannel->write_position = 0;
		  wschannel->bytes_needed = 2;  //minimum frame header size
		  ws->readyState = kws_websocket_open;
		  if (protocol) {
			  ws->protocol = (char*)malloc(strlen(protocol) + 1);
			  strcpy(ws->protocol, protocol);
		  }
		  if (extension) {
			  ws->extension = malloc(strlen(extension) + 1);
			  strcpy(ws->extension, extension);
		  }
		  //puts("OPEN");
		  pthread_mutex_lock(&wschannel->condition_mutex);
		  pthread_cond_signal(&wschannel->condition_cond);
		  pthread_mutex_unlock(&wschannel->condition_mutex);
		  return EXIT_SUCCESS;
	  }

	  handshake_failed:
	  return EXIT_FAILURE;
}

int kws_enqueque_message(kws_websocket_channel_t* wschannel, kws_message_event_t* evt) {

	kws_message_node_t* node = malloc(sizeof(kws_message_node_t));
	node->message_event = evt;
	node->next = NULL;
	int isempty = 0;
	pthread_mutex_lock(&wschannel->message_mutex);
	if (wschannel->message_list_head == NULL) {
		//empty list
		isempty = 1;
		wschannel->message_list_head =  node;
		wschannel->message_list_tail = node;
	}
	else {
		wschannel->message_list_tail->next = node;
		wschannel->message_list_tail = node;
	}
	pthread_mutex_unlock(&wschannel->message_mutex);

	if (isempty) {
		//signal for message arrived
		pthread_mutex_lock(&wschannel->condition_mutex);
		pthread_cond_signal(&wschannel->condition_cond);
		pthread_mutex_unlock(&wschannel->condition_mutex);
	}
	return EXIT_SUCCESS;
}

kws_message_node_t* kws_dequeque_message(kws_websocket_channel_t* wschannel) {
	kws_message_node_t* node = NULL;
	pthread_mutex_lock(&wschannel->message_mutex);
	node = wschannel->message_list_head;
	if (node) {
		wschannel->message_list_head = node->next;
		node->next = NULL;
	}
	pthread_mutex_unlock(&wschannel->message_mutex);
	return node;
}

int kws_decode_message(kws_websocket* ws, int len){
	kws_websocket_channel_t* wschannel = ws->channel;

	// append data to headerbuffer
	memcpy(wschannel->decodebuffer + wschannel->write_position, wschannel->readbuffer, len);
	wschannel->write_position += len;

	uint8_t b;
	while (1) {
		if (wschannel->ws_state == kws_decode_header) {

			if (wschannel->write_position < wschannel->bytes_needed) {
				return EXIT_SUCCESS; //not enough data,
			}
			b = wschannel->decodebuffer[1];
			wschannel->bytes_needed = kws_rfc6455_decode_header_size(b);
			if (wschannel->write_position < wschannel->bytes_needed) {
				return EXIT_SUCCESS;  // not enough data
			}
			//opcode
			b = wschannel->decodebuffer[0];
			if ((b & 0x80) == 0) {
				// this client does not support CONTINUATION
				puts("CONTINUATION not supported");
			}
			wschannel->opCode = b & 0x7f;
			//payload length
			b = wschannel->decodebuffer[1];
			int mask_key = (b & 0x80) ? 1 : 0;
			b = b & 0x7f;
			if (b < 126) {
				wschannel->payload_size = b;
			} else if (b == 126) {
				//payload length is stored in next 2 bytes
				wschannel->payload_size = (int)kws_rfc6455_decode_payload_size((unsigned char*)wschannel->decodebuffer + 2, 2);
				mask_key = (mask_key ? 4 : 0);
			} else {
				//payload length is stored in next 8 bytes
				wschannel->payload_size = (int)kws_rfc6455_decode_payload_size((unsigned char*)wschannel->decodebuffer + 2, 8);
				mask_key = (mask_key ? 10 : 0);
			}
			//maskkey needed
			if (mask_key) {
				wschannel->mask_key = (char*)malloc(4);
				memcpy(wschannel->mask_key, wschannel->decodebuffer + mask_key, 4);
			}

			wschannel->ws_state = kws_decode_data;
			//remove header bytes
			memmove(wschannel->decodebuffer, wschannel->decodebuffer + wschannel->bytes_needed, wschannel->write_position - wschannel->bytes_needed);
			wschannel->write_position = wschannel->write_position - wschannel->bytes_needed;
			wschannel->bytes_needed = wschannel->payload_size;
			wschannel->msgbuffer = (char*)malloc(wschannel->payload_size +1); //add one more byte for string terminator
			wschannel->msgbuffer[wschannel->payload_size] = '\0';
		}
		if (wschannel->ws_state == kws_decode_data) {
			int datacopied = wschannel->write_position < wschannel->bytes_needed ? wschannel->write_position : wschannel->bytes_needed;
			memcpy(wschannel->msgbuffer, wschannel->decodebuffer, datacopied);
			wschannel->bytes_needed -= datacopied;

			if (wschannel->bytes_needed == 0) {
				//we got enough data, fire event
				if (wschannel->opCode == KWS_RFC6455_OPCODE_BINARY || wschannel->opCode == KWS_RFC6455_OPCODE_TEXT) {
					//puts("MESSAGE");
					kws_message_event_t* evt = malloc(sizeof(kws_message_event_t));
					evt->length = wschannel->payload_size;
					evt->type = (int)wschannel->opCode;
					evt->data = wschannel->msgbuffer;
					kws_enqueque_message(wschannel, evt);

				}
				else if (wschannel->opCode == KWS_RFC6455_OPCODE_PING) {
					//puts("PING");
					//send PONG
					kws_send_data_internal(wschannel, wschannel->msgbuffer, wschannel->payload_size, KWS_RFC6455_OPCODE_PONG);
				}
				else if (wschannel->opCode == KWS_RFC6455_OPCODE_CLOSE) {
					//puts("CLOSE");
					if (ws->readyState == kws_websocket_open) {
						// complete websocket close handshake by sending close frame to server
						ws->readyState = kws_websocket_closing;
						kws_send_data_internal(wschannel, wschannel->msgbuffer, wschannel->payload_size, KWS_RFC6455_OPCODE_CLOSE);
					}
					kws_message_event_t* evt = malloc(sizeof(kws_message_event_t));
					if (evt->length) {
						//close frame with close code
						evt->length = wschannel->payload_size;
						evt->data = wschannel->msgbuffer;
					}
					else {
						// close without code and reason
						evt->length = 2;
						evt->data = (char*)malloc(2);
					    memcpy(evt->data, KWS_RFC6455_CLOSE_NORMAL, 2);  // close code = 1005
					}
					evt->type = (int)wschannel->opCode;
					kws_enqueque_message(wschannel, evt);
					ws->readyState = kws_websocket_closed;
					//close the socket
				    kws_close_socket(ws->channel);
					return EXIT_SUCCESS;

				}

				// is there more data from next frame?
				if (wschannel->write_position > datacopied) {
					// next frame already received
					memmove(wschannel->decodebuffer, wschannel->decodebuffer + datacopied, datacopied);
					wschannel->write_position = wschannel->write_position - datacopied;
				}
				else {
					wschannel->write_position = 0;
				}
				wschannel->ws_state = kws_decode_header;
				wschannel->bytes_needed = 2;
			}
			else {
				//wait for more data
				return EXIT_SUCCESS;
			}
		}
	}
	return EXIT_SUCCESS;
}

void* kws_start_receive_thread(void* arg) {

	kws_websocket* ws = (kws_websocket*)arg;
	int len;
	int result;
	while(1) {
		if (ws->channel->sslconnection) {
			len = SSL_read(ws->channel->sslconnection, ws->channel->readbuffer, WEBSOCKET_MAX_READ_SIZE);
		}
		else {
			len = recv(ws->channel->socketref, ws->channel->readbuffer, WEBSOCKET_MAX_READ_SIZE, 0);
		}

		if (ws->channel->ws_state) {
			//
			//websocket already connected
			//
			if (len <= 0) {
				// socket closed, enqueue a closed event if hasn't
				if (ws->readyState != kws_websocket_closed) {
					//enqueque close event
					kws_message_event_t* evt = malloc(sizeof(kws_message_event_t));
					evt->type = KWS_RFC6455_OPCODE_CLOSE;
					evt->length = 2;
					evt->data = (char*) malloc(2);
					memcpy(evt->data, KWS_RFC6455_CLOSE_ABNORMAL, 2);
					kws_enqueque_message(ws->channel, evt);
					ws->readyState = kws_websocket_closed;
				}
				break;
			} else {
				ws->channel->readbuffer[len] = '\0';
				// data received, decode websocket frames
				if ((result = kws_decode_message(ws, len)) != EXIT_SUCCESS) {

					// decode websocket frame error
					// enqueue close event
					// close the connection
					if (ws->readyState != kws_websocket_closed) {
						kws_message_event_t* evt = malloc(sizeof(kws_message_event_t));
						evt->length = 2;
						evt->type = KWS_RFC6455_OPCODE_CLOSE;
						evt->data = (char*) malloc(2);
						memcpy(evt->data, KWS_RFC6455_CLOSE_ABNORMAL, 2);
						kws_enqueque_message(ws->channel, evt);
						ws->readyState = kws_websocket_closed;
					}
					//close the socket
					kws_close_socket(ws->channel);
					break;
				}
			}
		}
		else {
			//
			// websocket handshake in process
			//
			if (len <= 0) {
				// failed to receive data
				kws_handle_handshake_failed(ws);
				break;
			}
			else {
				ws->channel->readbuffer[len] = '\0';
				if (kws_process_http_response(ws, len) != EXIT_SUCCESS) {
					// websocket handshake failed
					kws_handle_handshake_failed(ws);
					//close the socket
					kws_close_socket(ws->channel);
					break;
				}
			}
		}
	}
	return ws;
}

int kws_websocket_handshake(kws_websocket* ws, const char* location, const char* protocol, const char* extension) {

	kws_websocket_channel_t* wschannel = ws->channel;
	//parse url
	char *scheme, *host, *port, *path;

	char *start, *end;
	int len;
	end = strstr(location, "://");
	if (end == NULL) {
		return EXIT_FAILURE;  // not a valid uri
	}
	len = end - location;
	scheme = wschannel->readbuffer;
	strncpy(scheme, location, len);

	if (strcmp(scheme, "ws") != 0 && strcmp(scheme, "wss") != 0) {
		return EXIT_FAILURE; // wrong scheme
	}
	host = wschannel->readbuffer + 5;
	//move pointer over "://"
	start = end + 3;
	end = strchr(start, ':');
	if (end == NULL) {
		// no prot number, use default http port
		port = (strcmp(scheme, "ws") == 0) ? "80" : "443";
		end = strchr(start, '/');
		if (end != NULL) {
			//has path
			len = end - start;
			strncpy(host, start, len);
			path = host + len + 2;
			strcpy(path, end);
		} else {
			// no path
			strcpy(host, start);
			path = "/";
		}
	} else {
		len = end - start;
		strncpy(host, start, len);
		port = host + len + 2;
		start = end + 1;
		end = strchr(start, '/');
		if (end != NULL) {
			len = end - start;
			strncpy(port, start, len);
			path = port + len + 2;
			strcpy(path, end);
		} else {
			strcpy(port, start);
			path = "/";
		}
	}

	//printf("CONNECT: [%s]://[%s]:[%s][%s]", scheme, host, port, path);

	int status;
	struct addrinfo host_info; // The struct that getaddrinfo() fills up with data.
	struct addrinfo *host_info_list; // Pointer to the to the linked list of host_info's.

	memset(&host_info, 0, sizeof host_info);

	host_info.ai_family = AF_UNSPEC;   // IP version not specified. Can be both.
	host_info.ai_socktype = SOCK_STREAM; // Use SOCK_STREAM for TCP or SOCK_DGRAM for UDP.

	status = getaddrinfo(host, port, &host_info, &host_info_list);
	if (status != 0) {
		return EXIT_FAILURE; //Invalid address
	}

	wschannel->socketref = socket(host_info_list->ai_family,
			host_info_list->ai_socktype, host_info_list->ai_protocol);
	if (wschannel->socketref == -1) {
		return EXIT_FAILURE; // failed to create socket
	}
	status = connect(wschannel->socketref, host_info_list->ai_addr,
			host_info_list->ai_addrlen);
	if (status == -1) {
		//printf("Cannot connect to host - %s\n", host);
		return EXIT_FAILURE;
	}

	freeaddrinfo(host_info_list);

	//SSL connect
	if ((strcmp(scheme, "wss") == 0)) {
		// Register the error strings for libcrypto & libssl
		SSL_load_error_strings();
		// Register the available ciphers and digests
		SSL_library_init();

		// New context saying we are a client, and using SSL 2 or 3
		wschannel->sslcontext = SSL_CTX_new(SSLv23_client_method());
		if (wschannel->sslcontext == NULL) {
			kws_close_socket(wschannel);
			return EXIT_FAILURE;
		}
		// Create an SSL struct for the connection
		wschannel->sslconnection = SSL_new(wschannel->sslcontext);
		if (wschannel->sslconnection == NULL){
			kws_close_socket(wschannel);
			return EXIT_FAILURE;
		}

		// Connect the SSL struct to our connection
		if (!SSL_set_fd(wschannel->sslconnection, wschannel->socketref)){
			kws_close_socket(wschannel);
			return EXIT_FAILURE;
		}

		// Initiate SSL handshake
		if (SSL_connect(wschannel->sslconnection) != 1){
			kws_close_socket(wschannel);
			return EXIT_FAILURE;
		}
	}

	//send websocket request
	//generate random webSocket Key
	unsigned char* randomKey = malloc(16);
	for (len = 0; len < 16; len++) {
		*(randomKey+len) = (unsigned char)rand();
	}
	wschannel->websocketkey = kws_base64_encode(randomKey, 16);
	free(randomKey);

	wschannel->decodebuffer = (char*)calloc(2048, 1);
    char* buf = wschannel->decodebuffer;
	// GET /echo HTTP/1.1
	strcpy(buf, "GET ");
	strcat(buf, path);
	strcat(buf, " HTTP/1.1\r\n");
	// Upgrade
	strcat(buf, "Upgrade: websocket\r\n");
	// Connect
	strcat(buf, CONNECTION_MESSAGE);
	strcat(buf, "\r\n");
	//Host
	strcat(buf, "Host: ");
	strcat(buf, host);
	strcat(buf, ":");
	strcat(buf, port);
	strcat(buf, "\r\n");
	// Origin
	strcat(buf, "Origin: http");
	if (strstr(location, "wss") == location)
		strcat(buf, "s");
	strcat(buf, "://");
	strcat(buf, host);
	strcat(buf, ":");
	strcat(buf, port);
	strcat(buf, "\r\n");
	//Sec-WebSocket-Key
	strcat(buf, "Sec-WebSocket-Key: ");
	strcat(buf, wschannel->websocketkey);
	strcat(buf, "\r\n");
	//Sec-WebSocket-Protocol
	if (protocol != NULL && strlen(protocol) > 0) {
		strcat(buf, WEBSOCKET_PROTOCOL);
		strcat(buf, ": ");
		strcat(buf, protocol);
		strcat(buf, "\r\n");
	}
	//Sec-WebSocket-Extensions
	if (extension != NULL && strlen(extension) > 0) {
		strcat(buf, WEBSOCKET_EXTENSIONS);
		strcat(buf, ": ");
		strcat(buf, extension);
		strcat(buf, "\r\n");
	}
	//Sec-WebSocket-Version
	strcat(buf, "Pragma: no-cache\r\nCache-Control: no-cache\r\nSec-WebSocket-Version: 13\r\n\r\n");

	//printf("SEND: %s\n", buf);

	if (wschannel->sslconnection) {
		status = SSL_write(wschannel->sslconnection, buf, strlen(buf));
	}
	else {
	    status = send(wschannel->socketref, buf, strlen(buf), 0);
	}
	// start receiving response header
	wschannel->ws_state = kws_handshake;
	wschannel->decodebuffer[0] = '\0';
	pthread_create(&wschannel->recv_thread_id, NULL, &kws_start_receive_thread, (void*)ws);

	// wait WebSocket handshake to complete
	pthread_mutex_lock(&wschannel->condition_mutex);
	pthread_cond_wait(&wschannel->condition_cond, &wschannel->condition_mutex);
	pthread_mutex_unlock(&wschannel->condition_mutex);

	return (ws->readyState == kws_websocket_open) ? EXIT_SUCCESS : EXIT_FAILURE;
}

void kws_init_channel(kws_websocket_channel_t* wschannel) {
	pthread_mutex_init(&wschannel->message_mutex, NULL);
	pthread_mutex_init(&wschannel->condition_mutex, NULL);
	pthread_cond_init(&wschannel->condition_cond, NULL);
	wschannel->message_list_head = NULL;
	wschannel->message_list_tail = NULL;
	wschannel->mask_key = NULL;
	wschannel->websocketkey = NULL;
	wschannel->readbuffer = (char*) calloc(WEBSOCKET_MAX_READ_SIZE, 1);
	wschannel->decodebuffer = NULL;
	wschannel->sslconnection = NULL;
	wschannel->sslconnection = NULL;
}

void kws_free_channel(kws_websocket_channel_t* wschannel) {
	pthread_cond_destroy(&wschannel->condition_cond);
	pthread_mutex_destroy(&wschannel->condition_mutex);
	pthread_mutex_destroy(&wschannel->message_mutex);

	if (wschannel->decodebuffer) {
		free(wschannel->decodebuffer);
		wschannel->decodebuffer = NULL;
	}
	if (wschannel->readbuffer) {
		free(wschannel->readbuffer);
		wschannel->readbuffer = NULL;
	}
	if (wschannel->mask_key) {
		free(wschannel->mask_key);
		wschannel->mask_key = NULL;
	}
	if (wschannel->websocketkey) {
		free(wschannel->websocketkey);
		wschannel->websocketkey = NULL;
	}
	// clear message queue
	if (wschannel->message_list_head) {
		kws_message_node_t* node, *next;
		node = wschannel->message_list_head;
		while(node != NULL) {
			next = node->next;
			//free message event
			free(node->message_event->data);
			free(node->message_event);
			free(node);
			node = next;
		}
	}
}


kws_websocket* kws_websocket_new() {
	kws_websocket *ws = (kws_websocket*)malloc(sizeof(kws_websocket));
	ws->readyState = kws_websocket_connecting;
	return ws;
}

void kws_websocket_free(kws_websocket* ws) {
	if (ws->channel != NULL) {
		kws_free_channel(ws->channel);
		ws->channel = NULL;
	}
}

int kws_connect(kws_websocket* ws, const char* location, const char* protocol, const char* extensions) {

	if (ws->readyState != kws_websocket_connecting) {
		//websocket connect already called
		return EXIT_FAILURE;
	}
	//initial websocket_channel
	ws->channel = malloc(sizeof(kws_websocket_channel_t));
	kws_init_channel(ws->channel);

	int result = kws_websocket_handshake(ws, location, protocol, extensions);
	if (result != EXIT_SUCCESS) {
		kws_free_channel(ws->channel);
		ws->channel = NULL;
	}
	return result;
}

char* kws_protocol(kws_websocket* ws) {
	if (ws->readyState != kws_websocket_open) {
			return NULL;
		}
	return ws->protocol;
}
char* kws_extension(kws_websocket* ws) {
	if (ws->readyState != kws_websocket_open) {
			return NULL;
		}
	return ws->extension;
}

int kws_send_text(kws_websocket* ws, const char* text) {
	if (ws->readyState != kws_websocket_open) {
		return EXIT_FAILURE;
	}
	return kws_send_data_internal(ws->channel, text, strlen(text), KWS_RFC6455_OPCODE_TEXT);
}

int kws_send_binary(kws_websocket* ws, const char* binary, size_t size) {
	if (ws->readyState != kws_websocket_open) {
		return EXIT_FAILURE;
	}
	return kws_send_data_internal(ws->channel, binary, size, KWS_RFC6455_OPCODE_BINARY);
}

int kws_close_with_reason(kws_websocket* ws, int code, const char* reason) {

	char* payload = "";
	int payloadlength;
	if (code != 0) {
        //if code is present, it must equal to 1000 or in range 3000 to 4999
        if (code != 1000 && (code < 3000 || code > 4999)) {
            printf("code must equal to 1000 or in range 3000 to 4999\n");
            return EXIT_FAILURE;
        }
        if (reason != NULL && strlen(reason) > 0) {
            //UTF 8 encode reason
            if (strlen(reason) > 123) {
                printf("Reason is longer than 123 bytes\n");
                return EXIT_FAILURE;
            }
        }
	}
	else {
		//empty payload
		payloadlength = 0;
	}
	switch (ws->readyState) {
	case kws_websocket_connecting:
	case kws_websocket_closing:
		//close socket
		kws_close_socket(ws->channel);
		break;
	case kws_websocket_open:
		//send close frame
		ws->readyState = kws_websocket_closing;
		if (code != 0) {
	        payloadlength = 2 + strlen(reason);
	        payload = (char*)malloc(payloadlength);
	        kws_rfc6455_write_payload_length(payload, payloadlength, 2);
	        memcpy(payload + 2, reason, payloadlength - 2);
		}
		kws_send_data_internal(ws->channel, payload, payloadlength, KWS_RFC6455_OPCODE_CLOSE);
		if (code != 0)
				free(payload);
		break;
	default:
		break;
	}

	return EXIT_SUCCESS;
}


int kws_close(kws_websocket* ws) {
	return kws_close_with_reason(ws, 0, "");
}

kws_message_event_t* kws_recv(kws_websocket* ws) {
	kws_message_node_t* node = kws_dequeque_message(ws->channel);
	if (!node) {
		//no message yet, wait
		pthread_mutex_lock(&ws->channel->condition_mutex);
		pthread_cond_wait(&ws->channel->condition_cond, &ws->channel->condition_mutex);
		pthread_mutex_unlock(&ws->channel->condition_mutex);
	}
	node = kws_dequeque_message(ws->channel);
	if (node) {
		kws_message_event_t* evt = node->message_event;
		free(node);
		node = NULL;
		if (evt->type == KWS_RFC6455_OPCODE_CLOSE) {
			//websocket closed free channel
			if (ws->channel->socketref) {
				kws_close_socket(ws->channel);
				ws->channel->socketref = 0;
			}
			kws_free_channel(ws->channel);
			ws->channel = NULL;
		}
		return evt;
	}
	else {
		return NULL;
	}
}
