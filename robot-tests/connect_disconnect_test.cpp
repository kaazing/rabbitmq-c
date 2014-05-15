#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtest/gtest.h>

#include <amqp.h>
#include <amqp_tcp_socket.h>

TEST(CONNECT_DISCONNECT_TEST, tcp_connect_success) {
	amqp_connection_state_t conn;
	amqp_socket_t *socket = NULL;
	int status;
	conn = amqp_new_connection();
	ASSERT_TRUE(conn != NULL);

	socket = amqp_tcp_socket_new(conn);
	ASSERT_TRUE(socket != NULL);

	status = amqp_socket_open(socket, "localhost", 8001);

	ASSERT_EQ(AMQP_STATUS_OK, status);
}

TEST(CONNECT_DISCONNECT_TEST, amqp_connect_disconnect_success) {
	amqp_connection_state_t conn;
	amqp_socket_t *socket = NULL;
	int status;
	conn = amqp_new_connection();
	ASSERT_TRUE(conn != NULL);

	socket = amqp_tcp_socket_new(conn);
	ASSERT_TRUE(socket != NULL);

	status = amqp_socket_open(socket, "localhost", 8001);

	ASSERT_EQ(AMQP_STATUS_OK, status);
	amqp_rpc_reply_t  connect_reply = amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest");
	ASSERT_TRUE(AMQP_RESPONSE_NORMAL == connect_reply.reply_type);

	amqp_rpc_reply_t  disconnect_reply = amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
	ASSERT_TRUE(AMQP_RESPONSE_NORMAL == disconnect_reply.reply_type);

}
