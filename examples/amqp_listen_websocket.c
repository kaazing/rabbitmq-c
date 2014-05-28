/* vim:set ft=c ts=2 sw=2 sts=2 et cindent: */
/*
 * amqp_listen_websocket.c
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

#include <assert.h>

#include "utils.h"

int main(int argc, char const *const *argv)
{
  char const *url;
  int status;
  char const *exchange;
  char const *bindingkey;
  amqp_socket_t *socket = NULL;
  amqp_connection_state_t conn;

  amqp_bytes_t queuename;

  if (argc < 4) {
    fprintf(stderr, "Usage: amqp_listen_websocket url exchange bindingkey\n");
    return 1;
  }

  url = argv[1];
  exchange = argv[2];
  bindingkey = argv[3];

  /* Initialize AMQP connection object */
  conn = amqp_new_connection();

  /*
   * Initialize underlying transport object
   * We are using WebSocket as a transport protocol for AMQP messaging
   */
  socket = amqp_websocket_new(conn);
  if (!socket) {
    die("creating WebSocket");
  }

  /* Establish WebSocket connection */
  status = amqp_websocket_open(socket, url);
  if (status) {
    die("opening WebSocket connection");
  }

  /* Establish AMQP connection against the backend broker */
  die_on_amqp_error(amqp_login(conn, "/", 0, AMQP_DEFAULT_FRAME_SIZE, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest"),
                    "Logging in");

  /* Open Channel
   * To get the status of call to open channel, amqp_get_rpc_reply should be used
   * amqp_get_rpc_reply() returns the most recent amqp_rpc_reply_t instance corresponding
   * to such an API operation for the given connection.
   */
  amqp_channel_open(conn, 1);
  die_on_amqp_error(amqp_get_rpc_reply(conn), "Opening channel");

  {
	/* Declare Queue
	 * In this case we are providing empty name that results in server creating a unique
	 * queue name and sending it to the client
	 */
    amqp_queue_declare_ok_t *r = amqp_queue_declare(conn, 1, amqp_empty_bytes, 0, 0, 0, 1,
                                 amqp_empty_table);
    die_on_amqp_error(amqp_get_rpc_reply(conn), "Declaring queue");

    /* Get the queue name sent by the server */
    queuename = amqp_bytes_malloc_dup(r->queue);
    if (queuename.bytes == NULL) {
      fprintf(stderr, "Out of memory while copying queue name");
      return 1;
    }
  }

  /* Bind queue to an exchange */
  amqp_queue_bind(conn, 1, queuename, amqp_cstring_bytes(exchange), amqp_cstring_bytes(bindingkey),
                  amqp_empty_table);
  die_on_amqp_error(amqp_get_rpc_reply(conn), "Binding queue");

  /* Start a queue consumer.
   * This method asks the server to start a "consumer", which is a transient request
   * for messages from a specific queue.
   */
  amqp_basic_consume(conn, 1, queuename, amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
  die_on_amqp_error(amqp_get_rpc_reply(conn), "Consuming");

  {
    while (1) {
      amqp_rpc_reply_t res;
      amqp_envelope_t envelope;

      amqp_maybe_release_buffers(conn);

      /* Wait for and consume a message
       * The function waits for a basic.deliver method on any channel, upon receipt of
       * basic.deliver it reads that message, and returns.
       */
      res = amqp_consume_message(conn, &envelope, NULL, 0);

      if (AMQP_RESPONSE_NORMAL != res.reply_type) {
        break;
      }

      printf("Delivery %u, exchange %.*s routingkey %.*s\n",
             (unsigned) envelope.delivery_tag,
             (int) envelope.exchange.len, (char *) envelope.exchange.bytes,
             (int) envelope.routing_key.len, (char *) envelope.routing_key.bytes);

      if (envelope.message.properties._flags & AMQP_BASIC_CONTENT_TYPE_FLAG) {
        printf("Content-type: %.*s\n",
               (int) envelope.message.properties.content_type.len,
               (char *) envelope.message.properties.content_type.bytes);
      }

      printf("Message: %.*s\n",
                     (int) envelope.message.body.len,
                     (char *) envelope.message.body.bytes);

      amqp_destroy_envelope(&envelope);
    }
  }

  die_on_amqp_error(amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS), "Closing channel");
  die_on_amqp_error(amqp_connection_close(conn, AMQP_REPLY_SUCCESS), "Closing connection");
  die_on_error(amqp_destroy_connection(conn), "Ending connection");

  return 0;
}


