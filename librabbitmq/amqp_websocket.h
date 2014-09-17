/* vim:set ft=c ts=2 sw=2 sts=2 et cindent: */
/** \file */
/*
 * amqp_webSocket.h
 *
 *  Created on: Apr 28, 2014
 *      Author: pkhanal
 */

#ifndef AMQP_WEBSOCKET_H
#define AMQP_WEBSOCKET_H

#include <amqp.h>
#include <kaazing_websocket.h>

AMQP_BEGIN_DECLS

/**
 *
 * Creates a new amqp_socket_t object. The amqp_socket_t is the abstract base structure that encapsulates the
 * underlying transport layer based on the API used to create it. The underlying transport used is this case
 * is WebSocket. The function should be called after allocating and initializing a new amqp_connection_state_t
 * object via amqp_new_connection()
 *
 *
 * \return A new socket object or NULL if an error occurred.
 *
 */
AMQP_PUBLIC_FUNCTION
amqp_socket_t *
AMQP_CALL
amqp_websocket_new(amqp_connection_state_t state);

/**
 *
 * Returns the pointer to underlying kws_websocket_t object.
 *
 *
 * \return A kws_websocket_t object pointer.
 *
 */
AMQP_PUBLIC_FUNCTION
websocket_t *
AMQP_CALL
amqp_websocket_get(amqp_socket_t *self);

/**
 * Open a WebSocket connection. This function establishes the WebSocket connection to enable AMQP
 * messaging over the WebSocket connection.
 *
 * This function should be called after initializing connection using amqp_new_connection() and 
 * creating a WebSocket object using amqp_websocket_new().
 *
 * \param [in,out] self A socket object.
 * \param [in] url A connection url. The url is comprised of <scheme>://<host>[:port]/[path]
 *                 For example: ws://localhost:8001/amqp. For secure connection use wss as a scheme.
 *
 * \return AMQP_STATUS_OK on success, an amqp_status_enum on failure.
 *
 */
AMQP_PUBLIC_FUNCTION
int
AMQP_CALL
amqp_websocket_open(amqp_socket_t *self, const char *url);

AMQP_END_DECLS


#endif /* AMQP_WEBSOCKET_H */
