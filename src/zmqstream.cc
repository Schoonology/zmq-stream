#include <node.h>
#include <node_buffer.h>
#include <zmq.h>
#include <stdio.h>

#include "zmqstream.h"

using namespace v8;
using namespace node;

namespace zmqstream {
  // TODO: Is there any reason to have more than one Context?
  // Would that make managing blocking sockets (REQ, DEALER, PUSH) easier?
  ScopedContext gContext;
  Persistent<FunctionTemplate> Socket::constructor;

  ScopedContext::ScopedContext() {
    context = zmq_ctx_new();
  }

  ScopedContext::~ScopedContext() {
    zmq_ctx_destroy(context);
    printf("~Context\n");
  }

  Socket::Socket(int type) : ObjectWrap() {
    this->socket = zmq_socket(gContext.context, type);

    zmq_setsockopt(this->socket, ZMQ_IDENTITY, "TestClient", 10);
  }

  Socket::~Socket() {
    if (this->socket) {
      zmq_close(this->socket);
    }

    printf("~Socket\n");
  }

  Handle<Value> Socket::Close(const Arguments& args) {
    HandleScope scope;
    Socket *self = ObjectWrap::Unwrap<Socket>(args.This());

    if (self->socket == NULL) {
      return scope.Close(Undefined());
    }

    zmq_close(self->socket);
    self->socket = NULL;
    return scope.Close(Undefined());
  }

  //
  // TODO:
  //  * Handle highWaterMark and lowWaterMark options.
  //
  Handle<Value> Socket::New(const Arguments& args) {
    HandleScope scope;

    if (!args.IsConstructCall()) {
      Handle<Value> argv[1] = { args[0] };
      return constructor->GetFunction()->NewInstance(1, argv);
    }

    Handle<Object> options;

    if (args.Length() < 1 || !args[0]->IsObject()) {
      options = Object::New();
    } else {
      options = args[0]->ToObject();
    }

    Handle<Integer> type = options->Get(String::NewSymbol("type"))->ToInteger();

    // Creates a new instance object of this type and wraps it.
    Socket* obj = new Socket(type->Value());
    obj->Wrap(args.This());

    // Establishes initial property values.
    args.This()->Set(String::NewSymbol("type"), type);

    return args.This();
  }

  Handle<Value> Socket::Read(const Arguments& args) {
    HandleScope scope;
    Socket *self = ObjectWrap::Unwrap<Socket>(args.This());

    if (self->socket == NULL) {
      return ThrowException(Exception::ReferenceError(String::New("Socket is closed, and cannot be read from.")));
    }

    return scope.Close(Null());
  }

  //
  // write([Buffer]) -> Boolean
  //
  Handle<Value> Socket::Write(const Arguments& args) {
    HandleScope scope;
    Socket *self = ObjectWrap::Unwrap<Socket>(args.This());

    if (self->socket == NULL) {
      return ThrowException(Exception::ReferenceError(String::New("Socket is closed, and cannot be written to.")));
    }

    if (args.Length() != 1) {
      return ThrowException(Exception::TypeError(String::New("Expected write([Buffer])")));
    }
    if (!args[0]->IsArray()) {
      return ThrowException(Exception::TypeError(String::New("Expected write([Buffer])")));
    }

    Local<Object> frames = args[0]->ToObject();
    int length = frames->Get(String::New("length"))->ToInteger()->Value();
    int flags = ZMQ_DONTWAIT;
    int rc;

    for (int i = 0; i < length; i++) {
      if (!Buffer::HasInstance(frames->Get(i))) {
        return ThrowException(Exception::TypeError(String::New("Expected write([Buffer])")));
      }
    }

    for (int i = 0; i < length; i++) {
      Handle<Object> buffer = frames->Get(i)->ToObject();

      // TODO:
      //  * Real return value
      //  * Error handling
      //  * Reads
      //  * Other socket types
      //  * Testing
      //  * 'drain' event
      //  * Investigate caching in JS and sending one event loop's worth every tick.
      rc = zmq_send(self->socket, Buffer::Data(buffer), Buffer::Length(buffer), i < length - 1 ? ZMQ_SNDMORE | flags : flags);

      if (rc == -1) {
        if (zmq_errno() == EAGAIN) {
          return scope.Close(Boolean::New(0));
        } else {
          // TODO: Real error type.
          ThrowException(Exception::TypeError(String::New(zmq_strerror(zmq_errno()))));
        }
      }
    }

    return scope.Close(Boolean::New(1));
  }

  Handle<Value> Socket::Connect(const Arguments& args) {
    HandleScope scope;
    Socket *self = ObjectWrap::Unwrap<Socket>(args.This());

    if (self->socket == NULL) {
      return ThrowException(Exception::ReferenceError(String::New("Socket is closed, and cannot be connected.")));
    }

    if (args.Length() != 1 || !args[0]->IsString()) {
      return ThrowException(Exception::TypeError(String::New("No endpoint specified to connect to.")));
    }

    String::AsciiValue endpoint(args[0]->ToString());

    zmq_connect(self->socket, *endpoint);

    return scope.Close(Undefined());
  }

  Handle<Value> Socket::Disconnect(const Arguments& args) {
    HandleScope scope;
    Socket *self = ObjectWrap::Unwrap<Socket>(args.This());

    if (self->socket == NULL) {
      return ThrowException(Exception::ReferenceError(String::New("Socket is closed, and cannot be disconnected.")));
    }

    if (args.Length() != 1 || !args[0]->IsString()) {
      return ThrowException(Exception::TypeError(String::New("No endpoint specified to disconnect from.")));
    }

    String::AsciiValue endpoint(args[0]->ToString());

    zmq_disconnect(self->socket, *endpoint);

    return scope.Close(Undefined());
  }

  Handle<Value> Socket::Bind(const Arguments& args) {
    HandleScope scope;
    Socket *self = ObjectWrap::Unwrap<Socket>(args.This());

    if (self->socket == NULL) {
      return ThrowException(Exception::ReferenceError(String::New("Socket is closed, and cannot be bound.")));
    }

    if (args.Length() != 1 || !args[0]->IsString()) {
      return ThrowException(Exception::TypeError(String::New("No endpoint specified to bind to.")));
    }

    String::AsciiValue endpoint(args[0]->ToString());

    zmq_bind(self->socket, *endpoint);

    return scope.Close(Undefined());
  }

  Handle<Value> Socket::Unbind(const Arguments& args) {
    HandleScope scope;
    Socket *self = ObjectWrap::Unwrap<Socket>(args.This());

    if (self->socket == NULL) {
      return ThrowException(Exception::ReferenceError(String::New("Socket is closed, and cannot be unbound.")));
    }

    if (args.Length() != 1 || !args[0]->IsString()) {
      return ThrowException(Exception::TypeError(String::New("No endpoint specified to unbind from.")));
    }

    String::AsciiValue endpoint(args[0]->ToString());

    zmq_unbind(self->socket, *endpoint);

    return scope.Close(Undefined());
  }

  void Socket::Install(Handle<Object> target) {
    HandleScope scope;

    constructor = Persistent<FunctionTemplate>::New(FunctionTemplate::New(New));

    // ObjectWrap uses the first internal field to store the wrapped pointer.
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(String::NewSymbol("Socket"));

    // Add all prototype methods, getters and setters here.
    NODE_SET_PROTOTYPE_METHOD(constructor, "read", Read);
    NODE_SET_PROTOTYPE_METHOD(constructor, "write", Write);
    NODE_SET_PROTOTYPE_METHOD(constructor, "close", Close);
    NODE_SET_PROTOTYPE_METHOD(constructor, "connect", Connect);
    NODE_SET_PROTOTYPE_METHOD(constructor, "disconnect", Disconnect);
    NODE_SET_PROTOTYPE_METHOD(constructor, "bind", Bind);
    NODE_SET_PROTOTYPE_METHOD(constructor, "unbind", Unbind);

    Local<Object> Type = Object::New();
    ZMQ_DEFINE_CONSTANT(Type, "REQ", ZMQ_REQ);
    ZMQ_DEFINE_CONSTANT(Type, "REP", ZMQ_REP);
    ZMQ_DEFINE_CONSTANT(Type, "DEALER", ZMQ_DEALER);
    ZMQ_DEFINE_CONSTANT(Type, "ROUTER", ZMQ_ROUTER);
    ZMQ_DEFINE_CONSTANT(Type, "PUB", ZMQ_PUB);
    ZMQ_DEFINE_CONSTANT(Type, "SUB", ZMQ_SUB);
    ZMQ_DEFINE_CONSTANT(Type, "XPUB", ZMQ_XPUB);
    ZMQ_DEFINE_CONSTANT(Type, "XSUB", ZMQ_XSUB);
    ZMQ_DEFINE_CONSTANT(Type, "PUSH", ZMQ_PUSH);
    ZMQ_DEFINE_CONSTANT(Type, "PULL", ZMQ_PULL);
    ZMQ_DEFINE_CONSTANT(Type, "PAIR", ZMQ_PAIR);

    target->Set(String::NewSymbol("Type"), Type, static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete));
    // This has to be last, otherwise the properties won't show up on the object in JavaScript.
    target->Set(String::NewSymbol("Socket"), constructor->GetFunction());

    // TODO: Ensure cleanup like so:
    // AtExit(Cleanup, NULL);
  }

  NODE_MODULE(zmqstream, Socket::Install);
}
