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

#include "kws_websocket.h"

struct amqp_websocket_t {
	const struct amqp_socket_class_t *klass;
	int sockfd;
	kws_websocket *ws;
	void *buffer;
	size_t buffer_length;
	int internal_error;
};

static ssize_t
amqp_websocket_writev(void *base, struct iovec *iov, int iovcnt)
{
	/* TODO: implement */
}


static ssize_t
amqp_websocket_send(void *base, const void *buf, size_t len)
{
	/* Send data over WebSocket Connection */
	struct amqp_websocket_t *self = (struct amqp_websocket_t *)base;
	int result = kws_send_binary(self->ws, buf, len);
	return result;
}

static ssize_t
amqp_websocket_recv(void *base, void *buf, size_t len, int flags)
{
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
	struct websocket_t *ws = kws_websocket_new();
	int result = kws_connect(ws, url, NULL, NULL);
	if (0 == result) {
		self->ws = ws;
		return AMQP_STATUS_OK;
	}
	else {
		return result;
	}
}

static int
amqp_websocket_close(void *base)
{
	struct amqp_websocket_t *self = (struct amqp_websocket_t *)base;
	kws_close(self->ws);
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

	return (amqp_socket_t *)self;
}
