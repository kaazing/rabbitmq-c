/*
 * connect_test.cpp
 *
 *  Created on: May 9, 2014
 *      Author: pkhanal
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtest/gtest.h>

#include <amqp.h>
#include <amqp_tcp_socket.h>

TEST(CONNECT_TEST, connect_success) {
	amqp_connection_state_t conn;
	amqp_socket_t *socket = NULL;
	conn = amqp_new_connection();
	ASSERT_TRUE(conn != NULL);

	socket = amqp_tcp_socket_new(conn);
	ASSERT_TRUE(socket != NULL);
}
