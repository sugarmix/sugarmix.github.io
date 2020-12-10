// clang rpc.pb-c.c rpc_server.c -lrabbitmq -lprotobuf-c -o rpc_server

#include "rpc.pb-c.h"
#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char const *const *argv) {

  amqp_connection_state_t conn = amqp_new_connection();
  amqp_socket_t *socket = amqp_tcp_socket_new(conn);
  amqp_socket_open(socket, "localhost", 5672);

  amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest");

  const amqp_channel_t channel = 1;
  amqp_channel_open(conn, channel);

  const amqp_bytes_t queue = amqp_cstring_bytes("rpc_server_queue");
  // durable = 0,auto_delete = 0
  amqp_queue_declare(conn, channel, queue, 0, 1, 0, 0, amqp_empty_table);
  // prefetch = 1
  amqp_basic_qos(conn, channel, 0, 1, 0);
  // no_ack = 0
  amqp_basic_consume(conn, channel, queue, amqp_empty_bytes, 0, 0, 0,
                     amqp_empty_table);
  srand(time(0));
  const amqp_bytes_t exchange = amqp_cstring_bytes("amq.direct");
  for (;;) {
    amqp_rpc_reply_t ret;
    amqp_envelope_t envelope;
    amqp_maybe_release_buffers(conn);

    ret = amqp_consume_message(conn, &envelope, NULL, 0);

    int process_time = rand() & 0x0F;
    if (AMQP_RESPONSE_NORMAL == ret.reply_type) {

      Request *request;
      request = request__unpack(NULL, envelope.message.body.len,
                                envelope.message.body.bytes);
      amqp_destroy_envelope(&envelope);

      printf("Received: reply_to = %s\n", request->reply_to);
      printf("Received: correlation_id = %s\n", request->correlation_id);
      printf("Received: request = %d\n", request->request);

      Response response = RESPONSE__INIT;
      response.correlation_id = request->correlation_id;
      response.response = request->request + 1;
      sleep(process_time);
      amqp_bytes_t body =
          amqp_bytes_malloc(response__get_packed_size(&response));

      response__pack(&response, body.bytes);
      amqp_basic_publish(conn, channel, exchange,
                         amqp_cstring_bytes(request->reply_to), 0, 0, NULL,
                         body);
      printf("Send: response = %d\n", response.response);
      amqp_bytes_free(body);
    }
    amqp_basic_ack(conn, channel, envelope.delivery_tag, 0);
    printf("Process Time = %d\n", process_time);
    printf("\n");
  }

  amqp_channel_close(conn, channel, AMQP_REPLY_SUCCESS);
  amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
  amqp_destroy_connection(conn);
}
