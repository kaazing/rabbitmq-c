
#ifndef KWS_WEBSOCKET_H_
#define KWS_WEBSOCKET_H_

static const int KWS_MESSAGE_TEXT = 1;
static const int KWS_MESSAGE_BINARY = 2;
static const int KWS_MESSAGE_CLOSED = 8;

enum kws_readystate {
	kws_websocket_connecting,
	kws_websocket_open,
	kws_websocket_closing,
	kws_websocket_closed
};

typedef struct kws_message_event_t_stct {
	int type;         // 1 - text, 2 - binary, 8 - closed
	char* data;            // message data
	int length;        // message data length
}kws_message_event_t;

typedef struct kws_websocket_t_stct kws_websocket;

kws_websocket* kws_websocket_new();
void kws_websocket_free(kws_websocket* ws);
int kws_connect(kws_websocket* ws, const char* location, const char* protocol, const char* extensions);
int kws_send_text(kws_websocket* ws, const char* text);
int kws_send_binary(kws_websocket* ws, const char* binary, size_t size);
int kws_close(kws_websocket* ws);
int kws_close_with_reason(kws_websocket* ws, int code, const char* reason);
kws_message_event_t* kws_recv(kws_websocket* ws);

char* kws_protocol(kws_websocket* ws);
char* kws_extension(kws_websocket* ws);

#endif /* KWS_WEBSOCKET_H_ */
