/* vim:set ft=c ts=2 sw=2 sts=2 et cindent: */

/*
 * amqp_sendstring_websocket.c
 *
 *  Created on: May 8, 2014
 *      Author: pkhanal
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdint.h>
#include <amqp_websocket.h>
#include <amqp.h>
#include <amqp_framing.h>

#include "utils.h"

int main(int argc, char const *const *argv)
{
  char const *url;
  int status;
  char const *exchange;
  char const *routingkey;
  char const *messagebody;
  amqp_socket_t *socket = NULL;
  amqp_connection_state_t conn;

  if (argc < 5) {
    fprintf(stderr, "Usage: amqp_sendstring_websocket url exchange routingkey messagebody\n");
    return 1;
  }

  url = argv[1];
  exchange = argv[2];
  routingkey = argv[3];
  messagebody = argv[4];

  conn = amqp_new_connection();

  socket = amqp_websocket_new(conn);
  if (!socket) {
    die("creating WebSocket");
  }

  status = amqp_websocket_open(socket, url);
  if (status) {
    die("opening WebSocket connection");
  }

  die_on_amqp_error(amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest"),
                    "Logging in");
  amqp_channel_open(conn, 1);
  die_on_amqp_error(amqp_get_rpc_reply(conn), "Opening channel");

  {
    amqp_basic_properties_t props;
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
    props.content_type = amqp_cstring_bytes("text/plain");
    props.delivery_mode = 2; /* persistent delivery mode */
    die_on_error(amqp_basic_publish(conn,
                                    1,
                                    amqp_cstring_bytes(exchange),
                                    amqp_cstring_bytes(routingkey),
                                    0,
                                    0,
                                    &props,
                                    amqp_cstring_bytes(messagebody)),
                 "Publishing");
  }

  die_on_amqp_error(amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS), "Closing channel");
  die_on_amqp_error(amqp_connection_close(conn, AMQP_REPLY_SUCCESS), "Closing connection");
  die_on_error(amqp_destroy_connection(conn), "Ending connection");
  return 0;
}

