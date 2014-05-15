/*
 * amqp_webSocket.h
 *
 *  Created on: Apr 28, 2014
 *      Author: pkhanal
 */

/**
 * A WebSocket connection.
 */

#ifndef AMQP_WEBSOCKET_H
#define AMQP_WEBSOCKET_H

#ifdef __cplusplus
#define AMQP_BEGIN_DECLS extern "C" {
#define AMQP_END_DECLS }
#else
#define AMQP_BEGIN_DECLS
#define AMQP_END_DECLS
#endif

#include <amqp.h>

AMQP_BEGIN_DECLS

/**
 * Create a new WebSocket. This function is used to enabled AMQP messaging over WebSocket.
 * Once WebSocket is created, Call amqp_websocket_open to establish the WebSocket connection.
 *
 * Call amqp_connection_close() to release socket resources.
 *
 * \return A new socket object or NULL if an error occurred.
 *
 */
AMQP_PUBLIC_FUNCTION
amqp_socket_t *
AMQP_CALL
amqp_websocket_new(amqp_connection_state_t state);

/**
 * Open a WebSocket connection. This function establishes the WebSocket connection to enable AMQP
 * messaging over the WebSocket connection.
 *
 * This function should be called after initializing connection using amqp_new_connection(),
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
