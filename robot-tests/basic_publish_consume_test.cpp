/*
 * basic_publish_consume_test.cpp
 *
 *  Created on: May 14, 2014
 *      Author: pkhanal
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtest/gtest.h>

#include <amqp.h>
#include <amqp_tcp_socket.h>

TEST(PUBLISH_CONSUME_TEST, basic_publish_success) {
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

	amqp_channel_open(conn, 1);
	amqp_rpc_reply_t  open_channel_reply = amqp_get_rpc_reply(conn);
	ASSERT_TRUE(AMQP_RESPONSE_NORMAL == open_channel_reply.reply_type);

	amqp_basic_properties_t props;
	props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
	props.content_type = amqp_cstring_bytes("text/plain");
	props.delivery_mode = 2; /* persistent delivery mode */
	status = amqp_basic_publish(conn, 1, amqp_cstring_bytes("amq.direct"), amqp_cstring_bytes("test"), 0, 0, &props, amqp_cstring_bytes("hello world"));
	ASSERT_EQ(AMQP_STATUS_OK, status);

	amqp_rpc_reply_t  close_channel_reply = amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
	ASSERT_TRUE(AMQP_RESPONSE_NORMAL == close_channel_reply.reply_type);

	amqp_rpc_reply_t  disconnect_reply = amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
	ASSERT_TRUE(AMQP_RESPONSE_NORMAL == disconnect_reply.reply_type);
}

TEST(PUBLISH_CONSUME_TEST, basic_consume_success) {
	amqp_connection_state_t conn;
	amqp_socket_t *socket = NULL;
	int status;
	amqp_bytes_t queuename;
	conn = amqp_new_connection();
	ASSERT_TRUE(conn != NULL);

	socket = amqp_tcp_socket_new(conn);
	ASSERT_TRUE(socket != NULL);

	status = amqp_socket_open(socket, "localhost", 8001);

	ASSERT_EQ(AMQP_STATUS_OK, status);
	amqp_rpc_reply_t  connect_reply = amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest");
	ASSERT_TRUE(AMQP_RESPONSE_NORMAL == connect_reply.reply_type);

	amqp_channel_open(conn, 1);
	amqp_rpc_reply_t  open_channel_reply = amqp_get_rpc_reply(conn);
	ASSERT_TRUE(AMQP_RESPONSE_NORMAL == open_channel_reply.reply_type);

	amqp_queue_declare_ok_t *r = amqp_queue_declare(conn, 1, amqp_empty_bytes, 0, 0, 0, 1, amqp_empty_table);
	amqp_rpc_reply_t  declare_queue_reply = amqp_get_rpc_reply(conn);
	ASSERT_TRUE(AMQP_RESPONSE_NORMAL == declare_queue_reply.reply_type);
	queuename = amqp_bytes_malloc_dup(r->queue);
	ASSERT_TRUE(queuename.bytes != NULL);

	amqp_queue_bind(conn, 1, queuename, amqp_cstring_bytes("amq.direct"), amqp_cstring_bytes("test"), amqp_empty_table);
	amqp_rpc_reply_t  bind_queue_reply = amqp_get_rpc_reply(conn);
	ASSERT_TRUE(AMQP_RESPONSE_NORMAL == bind_queue_reply.reply_type);

	amqp_basic_consume(conn, 1, queuename, amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
	amqp_rpc_reply_t  basic_consume_reply = amqp_get_rpc_reply(conn);
	ASSERT_TRUE(AMQP_RESPONSE_NORMAL == basic_consume_reply.reply_type);

	amqp_rpc_reply_t res;
	amqp_envelope_t envelope;
	res = amqp_consume_message(conn, &envelope, NULL, 0);
	ASSERT_TRUE(AMQP_RESPONSE_NORMAL == res.reply_type);
	ASSERT_EQ(1, envelope.delivery_tag);
	ASSERT_EQ(11, envelope.message.body.len);
	/* TODO: validate the message body, content-type */

	amqp_rpc_reply_t  close_channel_reply = amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
	ASSERT_TRUE(AMQP_RESPONSE_NORMAL == close_channel_reply.reply_type);

	amqp_rpc_reply_t  disconnect_reply = amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
	ASSERT_TRUE(AMQP_RESPONSE_NORMAL == disconnect_reply.reply_type);
}


