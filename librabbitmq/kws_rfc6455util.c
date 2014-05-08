
#include <string.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include "kws_rfc6455util.h"

#define IS_BIG_ENDIAN (*(uint16_t *)"\0\xff" < 0x100)
/*
 * websocket handshake functions
 */

static const char* WEBSOCKET_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"; // length is 36 bytes

char *kws_base64_encode(const unsigned char *input, int length) {
	BIO *bmem, *b64;
	BUF_MEM *bptr;

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	BIO_write(b64, input, length);
	BIO_flush(b64);
	BIO_get_mem_ptr(b64, &bptr);

	char *buff = (char *) malloc(bptr->length);
	memcpy(buff, bptr->data, bptr->length - 1);
	buff[bptr->length - 1] = 0;

	BIO_free_all(b64);

	return buff;
}

/**
 * Compute the Sec-WebSocket-Accept key (RFC-6455)
 *
 */
char* kws_rfc6455_accept_key_hash(char* key) {
	char input[strlen(key) + 37]; //append WEBSOCKET_GUID
	strcpy(input, key);
	strcat(input, WEBSOCKET_GUID);
	unsigned char obuf[20];

	SHA1((unsigned char*)input, strlen(input), obuf);
	return kws_base64_encode(obuf, 20);
}


/*
 * WebSocekt encoding functions
 */

uint16_t rfc6455_htons(uint16_t number) {
	if ((IS_BIG_ENDIAN)) {
		return number;
	}
	unsigned char *np = (unsigned char *) &number;

	return ((uint16_t) np[0] << 8) | (uint16_t) np[1];
}

uint64_t rfc6455_htonl(uint64_t number) {
	if ((IS_BIG_ENDIAN)) {
		return number;
	}
	unsigned char *np = (unsigned char *) &number;

	return ((uint64_t) np[0] << 56) | ((uint64_t) np[1] << 48)
			| ((uint64_t) np[2] << 40) | ((uint64_t) np[3] << 32)
			| ((uint64_t) np[4] << 24) | ((uint64_t) np[5] << 16)
			| ((uint64_t) np[6] << 8) | (uint64_t) np[7];
}

// write payload length into RFC-6455 frame
int kws_rfc6455_write_payload_length(char* buffer, uint64_t payload_length, int size) {
	if (size == 2) {
		//short
		uint16_t sh = rfc6455_htons((uint16_t) payload_length);
		memcpy(buffer, (char*) &sh, 2);
	} else if (size == 8) {
		//long
		uint64_t i = rfc6455_htonl((uint64_t) payload_length);
		memcpy(buffer, (char*) &i, 8);
	} else {
		//error
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

// mask payload data
int rfc6455_mask(unsigned char* buffer, int position, int len) {
	int i;
	for (i = 0; i < len; i++) {
		buffer[position + i] = buffer[position + i] ^ buffer[position - 4 + (i % 4)];
	}
	return EXIT_SUCCESS;
}

// calculate how many bytes are required to store payload length
int rfc6455_calculate_length_size(unsigned int len) {
	if (len < 126) {
		return 0;
	} else if (len < 65535) {
		return 2;
	} else {
		return 8;
	}
}

// calculate RFC-6455 frame header size
int kws_rfc6455_calculate_header_size(int len, int mask) {
	return (2 + (mask ? 4 : 0) + rfc6455_calculate_length_size(len));
}

// RFC 6455 encoder
char* kws_rfc6455_encode(const char* payload, size_t payload_length, uint8_t opcode, int mask) {

	uint8_t fin = 1;

	int headerLength = kws_rfc6455_calculate_header_size(payload_length, mask);

	char* encoded = (char*) malloc(headerLength + payload_length);

	int position = 0;

	uint8_t b1 = (uint8_t) (fin ? 0x80 : 0x00);
	uint8_t b2 = (uint8_t) (mask ? 0x80 : 0x00);

	b1 |= opcode;
	b2 |= (payload_length < 126 ? payload_length : (payload_length < 65535 ? 126 : 127));

	encoded[position++] = b1;
	encoded[position++] = b2;

	//encode length
	if (payload_length >= 126 && payload_length < 65535) {
		kws_rfc6455_write_payload_length(encoded + position, payload_length, 2);
		position += 2;
	} else if (payload_length >= 65535) {
		kws_rfc6455_write_payload_length(encoded + position, 0, 4);
		position += 4;
		kws_rfc6455_write_payload_length(encoded + position, payload_length, 4);
		position += 4;
	}

	if (mask) {
		int b;
		for (b = 0; b < 4; b++)
			encoded[position++] = (char) rand();
	}
	//put message data
	int payloadPosition = position;
	if(payload_length > 0) {
		memcpy(encoded + position, payload, payload_length);

	    if (mask) {
		    rfc6455_mask((unsigned char*)encoded, payloadPosition, payload_length);
	    }
	}

	return encoded;
}

/*
 * RFC-6455 decoding function
 */

// calculate frame header size base on Byte #1
int kws_rfc6455_decode_header_size(uint8_t b) {
	int masked = (b & 0x80) ? 4 : 0;  // is payload masked?
	b = b & 0x7f;  // remove mask indicator
	if (b < 126) {
		// payload length is stored at lenbyte;
		return 2 + masked;
	} else if (b == 126) {
		// payload length is stored at next 2 bytes,
		return 4 + masked;
	} else {
		// payload length is stored at next 8 bytes
		return 10 + masked;
	}
}

// decode RFC-6455 payload size.
uint64_t kws_rfc6455_decode_payload_size(unsigned char* buf, size_t size) {
	if (size == 2) {
		//short
		uint16_t sh;
		memcpy((char*)&sh, buf, 2);
		return rfc6455_htons(sh);
	} else if (size == 8) {
		//long
		uint64_t l;
		memcpy((char*)&l, buf, 8);
		return rfc6455_htonl(l);
	} else {
		//error
		return -1;
	}
}
