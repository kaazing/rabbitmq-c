/*
 * amqp_webSocket.c
 *
 *  Created on: Apr 28, 2014
 *      Author: pkhanal
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "amqp_private.h"
#include "amqp_websocket.h"

#include <assert.h>
#include <stdlib.h>

struct amqp_websocket_t {
	const struct amqp_socket_class_t *klass;
	int sockfd;
	kws_websocket_t *ws;
	void *buffer;
	size_t buffer_length;
	int internal_error;
};

static ssize_t
amqp_websocket_send(void *base, const void *buf, size_t len)
{
	/* Send data over WebSocket Connection */
	struct amqp_websocket_t *self = (struct amqp_websocket_t *)base;
	int result = kws_send_binary(self->ws, buf, len);
	return result;
}

static ssize_t
amqp_websocket_writev(void *base, struct iovec *iov, int iovcnt)
{
	/* NOTE: Until the WebSocket library supports Scatter-Gather I/O,
	 * the buffers are combined into a single buffer  */
	/* Copying data from multiple memory locations to a single buffer is a bit efficient. */
	ssize_t length = 0;
	ssize_t offset;
	char *data = NULL;
	int i;

	for (i = 0; i < iovcnt; i++) {
		length += iov[i].iov_len;
	}

	data = malloc(length * sizeof(char));

	if (!data) {
		return AMQP_STATUS_NO_MEMORY;
	}

	offset = 0;
	for (i = 0; i < iovcnt; i++) {
		memcpy(&data[offset], iov[i].iov_base, iov[i].iov_len);
		offset += iov[i].iov_len;
	}
	return amqp_websocket_send(base, data, length);
}

static ssize_t
amqp_websocket_recv(void *base, void *buf, size_t len, int flags)
{
	/* TODO: Assuming flags = 0 */
	/* Receive message over WebSocket connection */
	struct amqp_websocket_t *self = (struct amqp_websocket_t *)base;
	kws_message_event_t *evt = kws_recv(self->ws);
	if (evt->type == KWS_MESSAGE_TEXT) {
		return AMQP_STATUS_SOCKET_ERROR;
	}
	else if (evt->type == KWS_MESSAGE_BINARY) {
		memcpy(buf, evt->data, evt->length);
		return evt->length;

	}else if (evt->type == KWS_MESSAGE_CLOSED) {
		return AMQP_STATUS_SOCKET_ERROR;

	} else {
		return AMQP_STATUS_SOCKET_ERROR;
	}
}

static int
amqp_websocket_open_internal(void *base, const char *url)
{
	/* Establish WebSocket Connection */
	struct amqp_websocket_t *self = (struct amqp_websocket_t *)base;
	return kws_connect(self->ws, url, NULL, NULL);
}

static int
amqp_websocket_close(void *base)
{
	struct amqp_websocket_t *self = (struct amqp_websocket_t *)base;
	return kws_close(self->ws);
}

static int
amqp_websocket_get_sockfd(void *base)
{
	/* TODO: implement */
}

static void
amqp_websocket_delete(void *base)
{
	struct amqp_websocket_t *self = (struct amqp_websocket_t *)base;
	kws_websocket_free(self->ws);
	free(self->buffer);
	free(self);
}

static const struct amqp_socket_class_t amqp_webSocket_class = {
  amqp_websocket_writev, /* writev */
  amqp_websocket_send, /* send */
  amqp_websocket_recv, /* recv */
  amqp_websocket_open_internal, /* open */
  amqp_websocket_close, /* close */
  amqp_websocket_get_sockfd, /* get_sockfd */
  amqp_websocket_delete /* delete */
};

amqp_socket_t * amqp_websocket_new(amqp_connection_state_t state) {
	struct amqp_websocket_t *self = calloc(1, sizeof(*self));
	if (!self) {
		return NULL;
	}
	self->klass = &amqp_webSocket_class;
	self->sockfd = -1;

	amqp_set_socket(state, (amqp_socket_t *)self);

	kws_websocket_t *ws = kws_websocket_new();
	if (ws == NULL) {
		return NULL;
	}
	self->ws = ws;

	return (amqp_socket_t *)self;
}

kws_websocket_t * amqp_websocket_get(amqp_socket_t *base) {
	struct amqp_websocket_t *self = (struct amqp_websocket_t *)base;
	return self->ws;
}

int amqp_websocket_open(amqp_socket_t *self, const char *url) {
	assert(self);
	assert(self->klass->open);

	/* TODO: port is ignored internally.
	       host, port and path along with scheme are all available in url.*/
	return self->klass->open(self, url, 0, NULL);
}

