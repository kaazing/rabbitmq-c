
#ifndef KWS_WEBSOCKET_CHANNEL_H_
#define KWS_WEBSOCKET_CHANNEL_H_

#include <pthread.h>
#include "kws_websocket.h"

enum kws_decodestate {
	kws_handshake,
	kws_decode_header,
	kws_decode_data
};

typedef struct kws_message_node_t_stct {
	struct kws_message_event_t_stct* message_event;
	struct kws_message_node_t_stct* next;
}kws_message_node_t;

typedef struct kws_websocket_channel_t_stct {
	char* websocketkey;
	enum kws_decodestate ws_state;
	char* readbuffer;
	char* decodebuffer;
	char* msgbuffer;
	int write_position;
	int bytes_needed;

	uint8_t opCode;
	char* mask_key;
	size_t payload_size;

	kws_message_node_t* message_list_head;
	kws_message_node_t* message_list_tail;
	int socketref;
	SSL* sslconnection;
	SSL_CTX* sslcontext;

	pthread_t recv_thread_id;
	pthread_mutex_t message_mutex;
	pthread_mutex_t condition_mutex;
	pthread_cond_t  condition_cond;
} kws_websocket_channel_t;

typedef struct kws_websocket_t_stct {

	// public
	char* location; // url of connection (ws://echo.websocket.org)
	char* protocol; // list of server accepted protocol(s)
	char* extension; // list of server accepted extension(s)
	enum kws_readystate readyState; // state of the websocket
	kws_websocket_channel_t* channel;
}kws_websocket;
#endif /* KWS_WEBSOCKET_CHANNEL_H_ */
