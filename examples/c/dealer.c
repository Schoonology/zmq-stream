#include "czmq.h"

int sent = 0;
int received = 0;
int count = 1000;

int sendHandler(zloop_t *loop, zmq_pollitem_t *item, void *client) {
  zstr_sendm(client, "");
  zstr_sendf(client, "ping:%d", ++sent);
}

int recvHandler(zloop_t *loop, zmq_pollitem_t *item, void *client) {
  zmsg_t *msg = zmsg_recv(client);
  zmsg_dump(msg);
  zmsg_destroy(&msg);

  received++;

  if (received >= count) {
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
  void *client = zsocket_new(ctx, ZMQ_DEALER);
  zloop_t *loop = zloop_new();

  zsockopt_set_identity(client, "CExampleDealer");
  zsocket_connect(client, "ipc:///tmp/zmqtestbr");

  zloop_timer(loop, 0, count, sendHandler, client);

  zmq_pollitem_t items [] = { { client, 0, ZMQ_POLLIN, 0 } };
  zloop_poller(loop, items, recvHandler, client);

  zloop_start(loop);

  zloop_destroy(&loop);
  zctx_destroy(&ctx);
  return 0;
}
