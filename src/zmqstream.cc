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

  //
  // ## ScopedContext
  //
  // At the moment, there's a 1:1 relationship between Node processes and ZMQ contexts. This scope-based wrapper
  // is used to ensure that global context is managed properly over the life of that process.
  //
  ScopedContext::ScopedContext() {
    context = zmq_ctx_new();
    assert(context != 0);
  }

  ScopedContext::~ScopedContext() {
    assert(zmq_ctx_destroy(context) == 0 || zmq_errno() == EINTR);
  }

  //
  // ## Helpers
  //
  // These "throw" helpers bail the calling context immediately.
  //
  #define THROW(str) return ThrowException(Exception::Error(String::New(str)));
  #define THROW_REF(str) return ThrowException(Exception::ReferenceError(String::New(str)));
  #define THROW_TYPE(str) return ThrowException(Exception::TypeError(String::New(str)));

  //
  // ZMQ provides its own error messages, so we'll pass those through to V8.
  //
  #define ZMQ_THROW() return ThrowException(Exception::Error(String::New(zmq_strerror(zmq_errno()))));
  #define ZMQ_CHECK(rc) if (!isSuccessRC(rc)) return ThrowException(Exception::Error(String::New(zmq_strerror(zmq_errno()))));

  //
  // PUSH shorthand for v8::Array.
  //
  #define PUSH(a, v) a->Set(a->Length(), v);

  //
  // Because we use ZMQ in a non-blocking way, EAGAIN holds a special place in our hearts.
  //
  static inline bool isEAGAIN(int rc) {
    return rc == -1 && zmq_errno() == EAGAIN;
  }

  //
  // ZMQ returns -1 on failure, so this is a simple test for that _ignoring_ EAGAIN.
  //
  static inline bool isSuccessRC(int rc) {
    // Ignore EAGAIN. EAGAIN is handled in a sane, call-specific manner.
    return rc != -1 || isEAGAIN(rc);
  }

  //
  // ## Socket
  //
  // Much like the native `net` module, a ZMQStream socket (perhaps obviously) is really just a Duplex stream that
  // you can `connect`, `bind`, etc. just like a native ZMQ socket.
  //
  Socket::Socket(int type) : ObjectWrap() {
    this->socket = zmq_socket(gContext.context, type);
    assert(this->socket != 0);
  }

  Socket::~Socket() {
    if (this->socket) {
      assert(zmq_close(this->socket) == 0);
    }
  }


  //
  // ## Close `Close()`
  //
  // Closes the underlying ZMQ socket. _The stream should no longer be used!_
  //
  Handle<Value> Socket::Close(const Arguments& args) {
    HandleScope scope;
    Socket *self = ObjectWrap::Unwrap<Socket>(args.This());
    void* socket = self->socket;

    if (socket == NULL) {
      return scope.Close(Undefined());
    }

    self->socket = NULL;

    ZMQ_CHECK(zmq_close(socket));

    return scope.Close(Undefined());
  }

  //
  // ## Socket(options)
  //
  // Creates a new **options.type** ZMQ socket. Defaults to PAIR.
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

    // TODO
    ZMQ_CHECK(zmq_setsockopt(obj->socket, ZMQ_IDENTITY, "TestClient", 10));

    return args.This();
  }

  //
  // ## Read `Read(size)`
  //
  // Consumes a minimum of **size** messages of data from the ZMQ socket. If **size** is undefined, the entire
  // queue will be read and returned.
  //
  // If there is no data to consume, or if there are fewer bytes in the internal buffer than the size argument,
  // then null is returned, and a future 'readable' event will be emitted when more is available.
  //
  // Calling stream.read(0) is a no-op with no internal side effects, but can be used to test for Socket validity.
  //
  // Returns an Array of Messages, which are in turn Arrays of Frames as Node Buffers.
  //
  // NOTE: To reiterate, this Read returns a different format than the builtin Duplex, which is a single Buffer or
  // String. Additionally, there is no encoding support.
  //
  Handle<Value> Socket::Read(const Arguments& args) {
    HandleScope scope;
    Socket *self = ObjectWrap::Unwrap<Socket>(args.This());

    if (self->socket == NULL) {
      THROW_REF("Socket is closed, and cannot be read from.");
    }

    int size = -1;

    if (args.Length() > 0) {
      size = args[0]->ToInteger()->Value();
    }

    if (size == 0) {
      return scope.Close(Null());
    }

    int rc = 0;
    zmq_msg_t part;
    Handle<Array> messages = Array::New();
    Handle<Array> message = Array::New();

    do {
      ZMQ_CHECK(zmq_msg_init(&part));

      rc = zmq_msg_recv(&part, self->socket, ZMQ_DONTWAIT);
      ZMQ_CHECK(rc);

      if (!isEAGAIN(rc)) {
        // We want to continue, so clear `rc`.
        rc = 0;

        message->Set(message->Length(), Buffer::New(String::New((char*)zmq_msg_data(&part), zmq_msg_size(&part))));

        if (!zmq_msg_more(&part)) {
          messages->Set(messages->Length(), message);
          message = Array::New();
        }
      }

      ZMQ_CHECK(zmq_msg_close(&part));
    } while (rc == 0);

    if (messages->Length() == 0) {
      return scope.Close(Null());
    }

    return scope.Close(messages);
  }

  //
  // ## Write `Write(message)`
  //
  // Writes **message** to the ZMQ socket to be transmitted over the wire at some time in the future.
  //
  // Calling stream.write([]) is a no-op with no internal side effects, but can be used for test for Socket
  // validity.
  //
  // Returns true if **message** was queued successfully, or false if the buffer is full (see ZMQ_DONTWAIT/EAGAIN).
  //
  // NOTE: Unlike the builtin Duplex class, a return value of `false` indicates the write was _unsuccessful_, and
  // will need to be tried again.
  //
  Handle<Value> Socket::Write(const Arguments& args) {
    HandleScope scope;
    Socket *self = ObjectWrap::Unwrap<Socket>(args.This());

    if (self->socket == NULL) {
      THROW_REF("Socket is closed, and cannot be written to.");
    }

    if (args.Length() < 1 || !args[0]->IsArray()) {
      THROW_TYPE("No message specified.");
    }

    Local<Object> frames = args[0]->ToObject();
    int length = frames->Get(String::New("length"))->ToInteger()->Value();
    int flags = ZMQ_DONTWAIT;
    int rc;

    for (int i = 0; i < length; i++) {
      if (!Buffer::HasInstance(frames->Get(i))) {
        THROW_TYPE("Cannot write non-Buffer message part.");
      }
    }

    for (int i = 0; i < length; i++) {
      Handle<Object> buffer = frames->Get(i)->ToObject();

      // TODO:
      //  * Error handling
      //  * 'drain' event
      //  * Investigate caching in JS and sending one event loop's worth every tick.
      rc = zmq_send(self->socket, Buffer::Data(buffer), Buffer::Length(buffer), i < length - 1 ? ZMQ_SNDMORE | flags : flags);
      ZMQ_CHECK(rc);

      if (isEAGAIN(rc)) {
        return scope.Close(Boolean::New(0));
      }
    }

    return scope.Close(Boolean::New(1));
  }

  //
  // ## Connect `Connect(endpoint)`
  //
  // Connects the ZMQ socket to **endpoint**, expressed as a String.
  //
  // Connect is synchronous, and will throw an Error upon failure.
  //
  Handle<Value> Socket::Connect(const Arguments& args) {
    HandleScope scope;
    Socket *self = ObjectWrap::Unwrap<Socket>(args.This());

    if (self->socket == NULL) {
      THROW_REF("Socket is closed, and cannot be connected.");
    }

    if (args.Length() != 1 || !args[0]->IsString()) {
      THROW_TYPE("No endpoint specified to connect to.");
    }

    String::AsciiValue endpoint(args[0]->ToString());

    ZMQ_CHECK(zmq_connect(self->socket, *endpoint));

    return scope.Close(Undefined());
  }

  //
  // ## Disconnect `Disconnect(endpoint)`
  //
  // Disconnects the ZMQ socket from **endpoint**, expressed as a String.
  //
  // Disconnect is synchronous, and will throw an Error upon failure.
  //
  Handle<Value> Socket::Disconnect(const Arguments& args) {
    HandleScope scope;
    Socket *self = ObjectWrap::Unwrap<Socket>(args.This());

    if (self->socket == NULL) {
      THROW_REF("Socket is closed, and cannot be disconnected.");
    }

    if (args.Length() != 1 || !args[0]->IsString()) {
      THROW_TYPE("No endpoint specified to disconnect from.");
    }

    String::AsciiValue endpoint(args[0]->ToString());

    ZMQ_CHECK(zmq_disconnect(self->socket, *endpoint));

    return scope.Close(Undefined());
  }

  //
  // ## Bind `Bind(endpoint)`
  //
  // Binds the ZMQ socket to **endpoint**, expressed as a String.
  //
  // Bind is synchronous, and will throw an Error upon failure.
  //
  Handle<Value> Socket::Bind(const Arguments& args) {
    HandleScope scope;
    Socket *self = ObjectWrap::Unwrap<Socket>(args.This());

    if (self->socket == NULL) {
      THROW_REF("Socket is closed, and cannot be bound.");
    }

    if (args.Length() != 1 || !args[0]->IsString()) {
      THROW_TYPE("No endpoint specified to bind to.");
    }

    String::AsciiValue endpoint(args[0]->ToString());

    ZMQ_CHECK(zmq_bind(self->socket, *endpoint));

    return scope.Close(Undefined());
  }

  //
  // ## Unbind `Unbind(endpoint)`
  //
  // Unbinds the ZMQ socket from **endpoint**, expressed as a String.
  //
  // Unbind is synchronous, and will throw an Error upon failure.
  //
  Handle<Value> Socket::Unbind(const Arguments& args) {
    HandleScope scope;
    Socket *self = ObjectWrap::Unwrap<Socket>(args.This());

    if (self->socket == NULL) {
      THROW_REF("Socket is closed, and cannot be unbound.");
    }

    if (args.Length() != 1 || !args[0]->IsString()) {
      THROW_TYPE("No endpoint specified to unbind from.");
    }

    String::AsciiValue endpoint(args[0]->ToString());

    ZMQ_CHECK(zmq_unbind(self->socket, *endpoint));

    return scope.Close(Undefined());
  }

  //
  // Exports the Socket class within the module `target`.
  //
  void Socket::InstallExports(Handle<Object> target) {
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

  NODE_MODULE(zmqstream, Socket::InstallExports);
}
