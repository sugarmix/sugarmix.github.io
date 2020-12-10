// clang rpc.pb-c.c rpc_client.c -lrabbitmq -lprotobuf-c -luuid -o rpc_client

#include "rpc.pb-c.h"
#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <uuid/uuid.h>

char *amqp_bytes_cstring(amqp_bytes_t src) {
  char *dest = malloc(src.len + 1);
  dest[src.len] = '\0';
  memcpy(dest, src.bytes, src.len);
  return dest;
}

int main(int argc, char const *const *argv) {

  amqp_connection_state_t conn = amqp_new_connection();
  amqp_socket_t *socket = amqp_tcp_socket_new(conn);
  amqp_socket_open(socket, "localhost", 5672);

  amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest");

  const amqp_channel_t channel = 1;
  amqp_channel_open(conn, channel);

  // Declare Start
  const amqp_bytes_t request_queue = amqp_cstring_bytes("rpc_server_queue");
  amqp_queue_declare(conn, channel, request_queue, 0, 1, 0, 0,
                     amqp_empty_table);

  const amqp_bytes_t exchange = amqp_cstring_bytes("amq.direct");
  const amqp_bytes_t binding_key = amqp_cstring_bytes("rpc_request");
  amqp_queue_bind(conn, channel, request_queue, exchange, binding_key,
                  amqp_empty_table);

  amqp_queue_declare_ok_t *declare_ok = amqp_queue_declare(
      conn, channel, amqp_empty_bytes, 0, 0, 0, 1, amqp_empty_table);
  const amqp_bytes_t response_queue = declare_ok->queue;
  const amqp_bytes_t response_queue_binding_key = response_queue;
  amqp_queue_bind(conn, channel, response_queue, exchange,
                  response_queue_binding_key, amqp_empty_table);
  // Declare End

  uuid_t binuuid;
  uuid_generate_random(binuuid);

  // Send Request
  const amqp_bytes_t routing_key = amqp_cstring_bytes("rpc_request");

  Request request = REQUEST__INIT;
  request.correlation_id = malloc(37);
  uuid_unparse_upper(binuuid, request.correlation_id);

  request.reply_to = amqp_bytes_cstring(response_queue_binding_key);
  request.request = atoi(argv[1]);

  amqp_bytes_t body = amqp_bytes_malloc(request__get_packed_size(&request));
  request__pack(&request, body.bytes);
  amqp_basic_publish(conn, channel, exchange, routing_key, 0, 0, NULL, body);
  amqp_bytes_free(body);
  free(request.reply_to);
  // Request End

  amqp_basic_consume(conn, channel, response_queue, amqp_empty_bytes, 0, 0, 0,
                     amqp_empty_table);
  printf("Send: request = %d\n", request.request);
  for (;;) {
    amqp_rpc_reply_t ret;
    amqp_envelope_t envelope;
    amqp_maybe_release_buffers(conn);

    ret = amqp_consume_message(conn, &envelope, NULL, 0);

    int process_time = rand() & 0x0F;
    if (AMQP_RESPONSE_NORMAL == ret.reply_type) {
      Response *response;
      response = response__unpack(NULL, envelope.message.body.len,
                                  envelope.message.body.bytes);
      if (!strcmp(request.correlation_id, response->correlation_id)) {
        printf("Received: response = %d\n", response->response);
        break;
      }

      amqp_destroy_envelope(&envelope);
    }

    sleep(process_time);
    amqp_basic_ack(conn, channel, envelope.delivery_tag, 0);
  }

  amqp_channel_close(conn, channel, AMQP_REPLY_SUCCESS);
  amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
  amqp_destroy_connection(conn);
}
