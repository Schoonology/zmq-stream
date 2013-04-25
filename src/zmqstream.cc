#include <node.h>
#include <node_buffer.h>
#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  // Internal version of NODE_DEFINE_CONSTANT to allow for names without the ZMQ_ prefix.
  //
  #define ZMQ_DEFINE_CONSTANT(target, name, constant)                  \
    (target)->Set(                                                     \
      v8::String::NewSymbol(name),                                     \
      v8::Integer::New(constant),                                      \
      static_cast<v8::PropertyAttribute>(v8::ReadOnly|v8::DontDelete)  \
    );

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
  Socket::Socket(int type) : ObjectWrap(), shouldDrain(false), shouldReadable(true) {
    this->socket = zmq_socket(gContext.context, type);
    assert(this->socket != 0);

    uv_os_sock_t fd;
    size_t size = sizeof fd;
    zmq_getsockopt(this->socket, ZMQ_FD, &fd, &size);

    assert(uv_poll_init_socket(uv_default_loop(), &readableHandle, fd) == 0);
    readableHandle.data = this;
    assert(uv_poll_init_socket(uv_default_loop(), &writableHandle, fd) == 0);
    writableHandle.data = this;
    assert(uv_idle_init(uv_default_loop(), &idleHandle) == 0);
    idleHandle.data = this;
  }

  Socket::~Socket() {
    if (this->socket) {
      assert(zmq_close(this->socket) == 0);
    }

    uv_close((uv_handle_t*)&writableHandle, NULL);
    uv_close((uv_handle_t*)&readableHandle, NULL);
    uv_close((uv_handle_t*)&idleHandle, NULL);
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
    Socket* self = new Socket(type->Value());
    assert(self);
    self->Wrap(args.This());
    self->Ref();

    if (hwm > 0) {
      ZMQ_CHECK(zmq_setsockopt(self->socket, ZMQ_SNDHWM, &hwm, sizeof hwm));
      ZMQ_CHECK(zmq_setsockopt(self->socket, ZMQ_RCVHWM, &hwm, sizeof hwm));
    }

    // Establishes initial property values.
    args.This()->Set(String::NewSymbol("type"), type);

    // If we've had a super"class" provided, be sure to call its constructor.
    Handle<Value> super = constructor->Get(String::NewSymbol("super_"));
    if (super->IsFunction()) {
      super->ToObject()->CallAsFunction(args.This(), 0, NULL);
    }

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
    assert(self);

    void *socket = self->socket;

    uv_poll_stop(&self->readableHandle);
    uv_poll_stop(&self->writableHandle);
    uv_idle_stop(&self->idleHandle);

    if (socket == NULL) {
      return scope.Close(Undefined());
    }

    self->socket = NULL;
    self->Unref();

    ZMQ_CHECK(zmq_close(socket));

    return scope.Close(Undefined());
  }

  //
  // ## SetOption `SetOption(option, value)`
  //
  // Sets a ZMQ-specific option on the underlying ZMQ socket.
  //
  Handle<Value> Socket::SetOption(const Arguments& args) {
    HandleScope scope;
    Socket *self = THIS_TO_SOCKET(args.This());
    assert(self);

    if (self->socket == NULL) {
      THROW_REF("Socket is closed, and options cannot be set.");
    }

    if (args.Length() < 1 || !args[0]->IsNumber()) {
      THROW_TYPE("No option type specified.");
    }

    if (args.Length() < 2) {
      THROW_TYPE("No option value specified.");
    }

    int type = args[0]->Int32Value();
    int rc = 0;

    switch (type) {
      // char*
      case ZMQ_SUBSCRIBE:
      case ZMQ_UNSUBSCRIBE:
      case ZMQ_IDENTITY:
      case ZMQ_TCP_ACCEPT_FILTER:
        {
          size_t size = args[1]->ToString()->Length();
          void *value = malloc(size);
          assert(value);
          args[1]->ToString()->WriteAscii((char*)value, 0, size);

          rc = zmq_setsockopt(self->socket, type, value, size);

          free(value);
        }
        break;
      // int
      case ZMQ_RATE:
      case ZMQ_RECOVERY_IVL:
      case ZMQ_SNDBUF:
      case ZMQ_RCVBUF:
      case ZMQ_LINGER:
      case ZMQ_RECONNECT_IVL:
      case ZMQ_RECONNECT_IVL_MAX:
      case ZMQ_MULTICAST_HOPS:
      case ZMQ_RCVTIMEO:
      case ZMQ_SNDTIMEO:
      case ZMQ_TCP_KEEPALIVE:
      case ZMQ_TCP_KEEPALIVE_IDLE:
      case ZMQ_TCP_KEEPALIVE_CNT:
      case ZMQ_TCP_KEEPALIVE_INTVL:
        {
          int value = args[1]->Int32Value();

          rc = zmq_setsockopt(self->socket, type, &value, sizeof value);
        }
        break;
      // bool
      case ZMQ_IPV4ONLY:
      case ZMQ_DELAY_ATTACH_ON_CONNECT:
      case ZMQ_ROUTER_MANDATORY:
      case ZMQ_XPUB_VERBOSE:
        {
          bool value = args[1]->Int32Value() > 0 ? 1 : 0;

          rc = zmq_setsockopt(self->socket, type, &value, sizeof value);
        }
        break;
      // uint64_t
      case ZMQ_AFFINITY:
        {
          uint64_t value = args[1]->Uint32Value();

          rc = zmq_setsockopt(self->socket, type, &value, sizeof value);
        }
        break;
      // int64_t
      case ZMQ_MAXMSGSIZE:
        {
          int64_t value = args[1]->IntegerValue();

          rc = zmq_setsockopt(self->socket, type, &value, sizeof value);
        }
        break;
    }

    ZMQ_CHECK(rc);

    return scope.Close(Undefined());
  }

  //
  // ## GetOption `GetOption(option)`
  //
  // Retrieves a ZMQ-specific option from the underlying ZMQ socket.
  //
  Handle<Value> Socket::GetOption(const Arguments& args) {
    HandleScope scope;
    Socket *self = THIS_TO_SOCKET(args.This());
    assert(self);

    if (self->socket == NULL) {
      THROW_REF("Socket is closed, and options cannot be retrieved.");
    }

    if (args.Length() < 1 || !args[0]->IsNumber()) {
      THROW_TYPE("No option type specified.");
    }

    Handle<Value> retval;
    int type = args[0]->Int32Value();
    size_t size;
    int rc = 0;

    switch (type) {
      // char*
      case ZMQ_IDENTITY:
      case ZMQ_LAST_ENDPOINT:
        {
          size = 128;
          void *value = malloc(size);
          assert(value);
          rc = zmq_getsockopt(self->socket, type, value, &size);
          retval = String::New((char*)value, size);
          free(value);
        }
        break;
      // int
      case ZMQ_TYPE:
      case ZMQ_SNDHWM:
      case ZMQ_RCVHWM:
      case ZMQ_RATE:
      case ZMQ_RECOVERY_IVL:
      case ZMQ_SNDBUF:
      case ZMQ_RCVBUF:
      case ZMQ_LINGER:
      case ZMQ_RECONNECT_IVL:
      case ZMQ_RECONNECT_IVL_MAX:
      case ZMQ_BACKLOG:
      case ZMQ_MULTICAST_HOPS:
      case ZMQ_RCVTIMEO:
      case ZMQ_SNDTIMEO:
      // case ZMQ_FD:
      case ZMQ_EVENTS:
      case ZMQ_TCP_KEEPALIVE:
      case ZMQ_TCP_KEEPALIVE_IDLE:
      case ZMQ_TCP_KEEPALIVE_CNT:
      case ZMQ_TCP_KEEPALIVE_INTVL:
        {
          size = sizeof(int);
          int value;
          rc = zmq_getsockopt(self->socket, type, &value, &size);
          retval = Number::New(value);
        }
        break;
      // bool
      case ZMQ_RCVMORE:
      case ZMQ_IPV4ONLY:
      case ZMQ_DELAY_ATTACH_ON_CONNECT:
        {
          size = sizeof(bool);
          bool value;
          rc = zmq_getsockopt(self->socket, type, &value, &size);
          retval = Boolean::New(value);
        }
        break;
      // uint64_t
      case ZMQ_AFFINITY:
        {
          size = sizeof(uint64_t);
          uint64_t value;
          rc = zmq_getsockopt(self->socket, type, &value, &size);
          retval = Number::New(value);
        }
        break;
      // int64_t
      case ZMQ_MAXMSGSIZE:
        {
          size = sizeof(int64_t);
          int64_t value;
          rc = zmq_getsockopt(self->socket, type, &value, &size);
          retval = Number::New(value);
        }
        break;
    }

    ZMQ_CHECK(rc);

    return scope.Close(retval);
  }

  //
  // ## Read `Read(size)`
  //
  // Consumes a maximum of **size** messages of data from the ZMQ socket. If **size** is undefined, the entire
  // queue will be read and returned.
  //
  // If there is no data to consume then null is returned, and a future 'readable' event will be emitted when more is
  // available.
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
    assert(self);

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

        message->Set(message->Length(), Local<Object>::New(Buffer::New((char*)zmq_msg_data(&part), zmq_msg_size(&part))->handle_));

        if (!zmq_msg_more(&part)) {
          size--;
          messages->Set(messages->Length(), message);
          message = Array::New();
        }
      }

      ZMQ_CHECK(zmq_msg_close(&part));
    } while (rc == 0 && size != 0);

    // We've just called recv, and are required to check ZMQ_EVENTS.
    uv_idle_start(&self->idleHandle, Socket::Check);

    if (messages->Length() == 0) {
      self->shouldReadable = true;
      uv_poll_start(&self->readableHandle, UV_READABLE, Check);
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
    Socket *self = THIS_TO_SOCKET(args.This());
    assert(self);

    if (self->socket == NULL) {
      THROW_REF("Socket is closed, and cannot be written to.");
    }

    if (args.Length() < 1 || !args[0]->IsArray()) {
      THROW_TYPE("No message specified.");
    }

    Local<Object> frames = args[0]->ToObject();
    int length = frames->Get(String::New("length"))->ToInteger()->Value();
    Handle<Object> buffer;
    size_t size;
    int rc;

    for (int i = 0; i < length; i++) {
      if (!Buffer::HasInstance(frames->Get(i))) {
        THROW_TYPE("Cannot write non-Buffer message part.");
      }
    }

    // We're about to call send, and are required to check ZMQ_EVENTS.
    uv_idle_start(&self->idleHandle, Socket::Check);

    for (int i = 0; i < length; i++) {
      zmq_msg_t part;

      buffer = frames->Get(i)->ToObject();
      size = Buffer::Length(buffer);

      ZMQ_CHECK(zmq_msg_init_size(&part, size));
      memcpy(zmq_msg_data(&part), Buffer::Data(buffer), size);

      rc = zmq_msg_send(&part, self->socket, i < length - 1 ? ZMQ_SNDMORE | ZMQ_DONTWAIT : ZMQ_DONTWAIT);
      ZMQ_CHECK(rc);

      if (isEAGAIN(rc)) {
        uv_poll_start(&self->writableHandle, UV_WRITABLE, Check);
        self->shouldDrain = true;
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
    assert(self);

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
    assert(self);

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
    assert(self);

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
    assert(self);

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
  // A `uv_poll_cb` registered to facilitate generating `'readable'` and `'drain'` events.
  //
  void Socket::Check(uv_poll_t* handle, int status, int events) {
    assert(handle);
    assert(status == 0);

    Socket* self = (Socket*)handle->data;
    assert(self);

    Socket::Check(self);
  }

  //
  // ## Check
  //
  // A `uv_idle_cb` registered to facilitate generating `'readable'` and `'drain'` events.
  //
  void Socket::Check(uv_idle_t* handle, int status) {
    assert(handle);

    uv_idle_stop(handle);

    Socket* self = (Socket*)handle->data;
    assert(self);

    Socket::Check(self);
  }

  //
  // ## Check
  //
  // The actual workhorse responsible for checking ZMQ_EVENTS, firing `'readable'` and `'drain'` as appropriate.
  //
  void Socket::Check(Socket *self) {
    HandleScope scope;

    assert(self->socket);

    Handle<Value> jsObj = SOCKET_TO_THIS(self);
    assert(!jsObj.IsEmpty());
    if (!jsObj->IsObject()) {
      return;
    }

    Handle<Value> emit = jsObj->ToObject()->Get(String::NewSymbol("emit"));

    if (!emit->IsFunction()) {
      return;
    }

    int zmqEvents = 0;
    size_t size = sizeof zmqEvents;

    if (zmq_getsockopt(self->socket, ZMQ_EVENTS, &zmqEvents, &size) < 0) {
      printf("Problem checking actual socket state.\n");
      return;
    }

    if (self->shouldReadable && (zmqEvents & ZMQ_POLLIN)) {
      self->shouldReadable = false;
      uv_poll_stop(&self->readableHandle);
      Handle<Value> args[1] = { String::New("readable") };
      emit->ToObject()->CallAsFunction(jsObj->ToObject(), 1, args);
    }

    if (self->shouldDrain && (zmqEvents & ZMQ_POLLOUT)) {
      self->shouldDrain = false;
      uv_poll_stop(&self->writableHandle);
      Handle<Value> args[1] = { String::New("drain") };
      emit->ToObject()->CallAsFunction(jsObj->ToObject(), 1, args);
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
    NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "set", SetOption);
    NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "get", GetOption);
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

    // TODO: While SetOption and GetOption technically support any ZMQ_* option, we only want to export those constants
    // that make sense. For example, there's currently no way to set `io_threads`, so setting AFFINITY doesn't matter.
    // However, setting IDENTITY, SUBSCRIBE, and UNSUBSCRIBE are _required_ in a lot of applications.
    Local<Object> Option = Object::New();
    ZMQ_DEFINE_CONSTANT(Option, "TYPE", ZMQ_TYPE);
    ZMQ_DEFINE_CONSTANT(Option, "IDENTITY", ZMQ_IDENTITY);
    ZMQ_DEFINE_CONSTANT(Option, "SUBSCRIBE", ZMQ_SUBSCRIBE);
    ZMQ_DEFINE_CONSTANT(Option, "UNSUBSCRIBE", ZMQ_UNSUBSCRIBE);
    ZMQ_DEFINE_CONSTANT(Option, "LINGER", ZMQ_LINGER);
    target->Set(String::NewSymbol("Option"), Option, static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete));

    // This has to be last, otherwise the properties won't show up on the object in JavaScript.
    target->Set(String::NewSymbol("Socket"), constructor);
    target->Set(String::NewSymbol("createSocket"), constructor);

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
