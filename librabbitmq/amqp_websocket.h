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

AMQP_END_DECLS


#endif /* AMQP_WEBSOCKET_H */
