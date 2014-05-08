
#ifndef KWS_RFC6455UTIL_H_
#define KWS_RFC6455UTIL_H_

#include <stdint.h>
#include <stdlib.h>

static const uint8_t KWS_RFC6455_OPCODE_CONTINUATION = 0;
static const uint8_t KWS_RFC6455_OPCODE_TEXT = 1;
static const uint8_t KWS_RFC6455_OPCODE_BINARY = 2;
static const uint8_t KWS_RFC6455_OPCODE_CLOSE = 8;
static const uint8_t KWS_RFC6455_OPCODE_PING = 9;
static const uint8_t KWS_RFC6455_OPCODE_PONG = 10;

static const char KWS_RFC6455_CLOSE_NORMAL[] = { 0x03, 0xed};    //1005
static const char KWS_RFC6455_CLOSE_ABNORMAL[] = { 0x03, 0xee};  //1006

/* websocket handshake functions */
char *kws_base64_encode(const unsigned char *input, int length);
char* kws_rfc6455_accept_key_hash(char* key);

/* websocket encoding functions */
char* kws_rfc6455_encode(const char* payload, size_t len, uint8_t opcode, int mask);
int kws_rfc6455_calculate_header_size(int payload_length, int mask);
int kws_rfc6455_write_payload_length(char* buffer, uint64_t payload_length, int size);

/* websocket encoding functions */
int kws_rfc6455_decode_header_size(uint8_t lengthbyte);
uint64_t kws_rfc6455_decode_payload_size(unsigned char* buf, size_t size);
#endif /* KWS_RFC6455UTIL_H_ */
