#include "czmq.h"

int sent = 0;
int received = 0;
int count = 1000;

int recvHandler(zloop_t *loop, zmq_pollitem_t *item, void *client) {
  zmsg_t *msg = zmsg_recv(client);
  zmsg_dump(msg);

  zstr_sendm(client, zmsg_popstr(msg));

  zmsg_destroy(&msg);

  received++;

  zstr_sendm(client, "");
  zstr_sendf(client, "pong:%d", ++sent);

  if (sent >= count) {
    return -1;
  }

  return 0;
}

int main (int argc, char *argv [])
{
  if (argc == 2) {
    count = atoi(argv[1]);
  }

  zctx_t *ctx = zctx_new();
  void *client = zsocket_new(ctx, ZMQ_ROUTER);
  zloop_t *loop = zloop_new();

  zsocket_bind(client, "ipc:///tmp/zmqtestbr");

  zmq_pollitem_t items [] = { { client, 0, ZMQ_POLLIN, 0 } };
  zloop_poller(loop, items, recvHandler, client);

  zloop_start(loop);

  zloop_destroy(&loop);
  zctx_destroy(&ctx);
  return 0;
}
