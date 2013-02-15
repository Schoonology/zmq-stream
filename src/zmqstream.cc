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
  Persistent<Function> Socket::constructor;

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
  // Internal versions of Unwrap.
  //
  #define SOCKET_TO_THIS(obj) obj->handle_
  ; // For Sublime's syntax highlighter. Ignore.
  #define THIS_TO_SOCKET(obj) ObjectWrap::Unwrap<Socket>(obj)

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
  Socket::Socket(int type) : ObjectWrap(), drain(true), readable(false) {
    this->socket = zmq_socket(gContext.context, type);
    assert(this->socket != 0);

    assert(uv_idle_init(uv_default_loop(), &handle) == 0);
    handle.data = this;
  }

  Socket::~Socket() {
    if (this->socket) {
      assert(zmq_close(this->socket) == 0);
    }
  }

  //
  // ## Socket(options)
  //
  // Creates a new **options.type** ZMQ socket. Defaults to PAIR.
  //
  Handle<Value> Socket::New(const Arguments& args) {
    HandleScope scope;

    if (!args.IsConstructCall()) {
      Handle<Value> argv[1] = { args[0] };
      return constructor->NewInstance(1, argv);
    }

    Handle<Object> options;

    if (args.Length() < 1 || !args[0]->IsObject()) {
      options = Object::New();
    } else {
      options = args[0]->ToObject();
    }

    Handle<Integer> type = options->Get(String::NewSymbol("type"))->ToInteger();
    int32_t hwm = options->Get(String::NewSymbol("highWaterMark"))->ToInteger()->Int32Value();

    // Creates a new instance object of this type and wraps it.
    Socket* obj = new Socket(type->Value());
    obj->Wrap(args.This());

    if (hwm > 0) {
      ZMQ_CHECK(zmq_setsockopt(obj->socket, ZMQ_SNDHWM, &hwm, sizeof hwm));
      ZMQ_CHECK(zmq_setsockopt(obj->socket, ZMQ_RCVHWM, &hwm, sizeof hwm));
    }

    // Establishes initial property values.
    args.This()->Set(String::NewSymbol("type"), type);

    // If we've had a super"class" provided, be sure to call its constructor.
    Handle<Value> super = constructor->Get(String::NewSymbol("super_"));
    if (super->IsFunction()) {
      super->ToObject()->CallAsFunction(args.This(), 0, NULL);
    }

    // Establish our libuv handle and callback.
    uv_idle_start(&obj->handle, Check);

    // TODO
    ZMQ_CHECK(zmq_setsockopt(obj->socket, ZMQ_IDENTITY, "TestClient", 10));

    return args.This();
  }

  //
  // ## Close `Close()`
  //
  // Closes the underlying ZMQ socket. _The stream should no longer be used!_
  //
  Handle<Value> Socket::Close(const Arguments& args) {
    HandleScope scope;
    Socket *self = THIS_TO_SOCKET(args.This());
    void* socket = self->socket;

    uv_idle_stop(&self->handle);

    if (socket == NULL) {
      return scope.Close(Undefined());
    }

    self->socket = NULL;

    ZMQ_CHECK(zmq_close(socket));

    // TODO: We could destroy the entire Socket with `delete`, but that would change the contract, making the object
    // "near death", rather than accessible but invalid.
    //
    // delete self;

    return scope.Close(Undefined());
  }

  //
  // ## Read `Read(size)`
  //
  // Consumes a maximum of **size** messages of data from the ZMQ socket. If **size** is undefined, the entire
  // queue will be read and returned.
  //
  // If there is no data to consume, or if there are fewer bytes in the internal buffer than the size argument,
  // then null is returned, and a future 'readable' event will be emitted when more is available.
  //
  // Calling stream.read(0) is a no-op with no internal side effects, but can be used to test for Socket validity.
  //
  // Returns an Array of Messages, which are in turn Arrays of Frames as Node Buffers.
  //
  // NOTE: To reiterate, this Read returns a different amount and different format than the builtin Duplex, which
  // is a single Buffer or String of <= `size`. Because of this, there is no encoding support.
  //
  Handle<Value> Socket::Read(const Arguments& args) {
    HandleScope scope;
    Socket *self = THIS_TO_SOCKET(args.This());

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
          size--;
          messages->Set(messages->Length(), message);
          message = Array::New();
        }
      } else {
        self->readable = false;
      }

      ZMQ_CHECK(zmq_msg_close(&part));
    } while (rc == 0 && size != 0);

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
  // TODO: Investigate caching in JS and sending one event loop's worth every tick.
  //
  Handle<Value> Socket::Write(const Arguments& args) {
    HandleScope scope;
    Socket *self = THIS_TO_SOCKET(args.This());

    if (self->socket == NULL) {
      THROW_REF("Socket is closed, and cannot be written to.");
    }

    if (args.Length() < 1 || !args[0]->IsArray()) {
      THROW_TYPE("No message specified.");
    }

    Local<Object> frames = args[0]->ToObject();
    int length = frames->Get(String::New("length"))->ToInteger()->Value();
    Handle<Object> buffer;
    int rc;

    for (int i = 0; i < length; i++) {
      if (!Buffer::HasInstance(frames->Get(i))) {
        THROW_TYPE("Cannot write non-Buffer message part.");
      }
    }

    for (int i = 0; i < length; i++) {
      buffer = frames->Get(i)->ToObject();

      rc = zmq_send(self->socket, Buffer::Data(buffer), Buffer::Length(buffer), i < length - 1 ? ZMQ_SNDMORE | ZMQ_DONTWAIT : ZMQ_DONTWAIT);
      ZMQ_CHECK(rc);

      if (isEAGAIN(rc)) {
        self->drain = false;
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
    Socket *self = THIS_TO_SOCKET(args.This());

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
    Socket *self = THIS_TO_SOCKET(args.This());

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
    Socket *self = THIS_TO_SOCKET(args.This());

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
    Socket *self = THIS_TO_SOCKET(args.This());

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
  // ## Check
  //
  // To generate `'readable'` and `'drain'` events, we need to be polling our socket handles periodically. We
  // define that period to be once per event loop tick, and this is our libuv callback to handle that.
  //
  void Socket::Check(uv_idle_t* handle, int status) {
    assert(handle);

    Socket* self = (Socket*)handle->data;
    assert(self->socket);

    Handle<Value> jsObj = SOCKET_TO_THIS(self);
    if (!jsObj->IsObject()) {
      return;
    }

    Handle<Value> emit = jsObj->ToObject()->Get(String::NewSymbol("emit"));

    if (!emit->IsFunction()) {
      return;
    }

    if (!self->readable) {
      zmq_pollitem_t item;

      item.socket = self->socket;
      item.events = ZMQ_POLLIN;

      int rc = zmq_poll(&item, 1, 0);
      assert(rc != -1);

      if (rc > 0) {
        self->readable = true;
        Handle<Value> args[1] = { String::New("readable") };
        emit->ToObject()->CallAsFunction(jsObj->ToObject(), 1, args);
      }
    }

    if (!self->drain) {
      zmq_pollitem_t item;

      item.socket = self->socket;
      item.events = ZMQ_POLLOUT;

      int rc = zmq_poll(&item, 1, 0);
      assert(rc != -1);

      if (rc > 0) {
        self->drain = true;
        Handle<Value> args[1] = { String::New("drain") };
        emit->ToObject()->CallAsFunction(jsObj->ToObject(), 1, args);
      }
    }
  }

  //
  // ## Initialize
  //
  // Creates and populates the constructor Function and its prototype.
  //
  void Socket::Initialize() {
    Local<FunctionTemplate> constructorTemplate(FunctionTemplate::New(New));

    // ObjectWrap uses the first internal field to store the wrapped pointer.
    constructorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
    constructorTemplate->SetClassName(String::NewSymbol("Socket"));

    // Add all prototype methods, getters and setters here.
    NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "read", Read);
    NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "write", Write);
    NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "close", Close);
    NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "connect", Connect);
    NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "disconnect", Disconnect);
    NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "bind", Bind);
    NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "unbind", Unbind);

    constructor = Persistent<Function>::New(constructorTemplate->GetFunction());
  }

  //
  // ## InstallExports
  //
  // Exports the Socket class within the module `target`.
  //
  void Socket::InstallExports(Handle<Object> target) {
    HandleScope scope;

    Initialize();

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
    target->Set(String::NewSymbol("Socket"), constructor);

    int major, minor, patch;
    char version[100];

    zmq_version(&major, &minor, &patch);
    sprintf(version, "v%d.%d.%d", major, minor, patch);
    target->Set(String::NewSymbol("version"), String::New(version));

    // TODO: Ensure cleanup like so:
    // AtExit(Cleanup, NULL);
  }

  NODE_MODULE(zmqstream, Socket::InstallExports);
}
