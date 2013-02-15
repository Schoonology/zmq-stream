#ifndef ZMQSTREAM_H
#define ZMQSTREAM_H

#include <node.h>

namespace zmqstream {
  //
  // ## ScopedContext
  //
  // At the moment, there's a 1:1 relationship between Node processes and ZMQ contexts. This scope-based wrapper
  // is used to ensure that global context is managed properly over the life of that process.
  //
  class ScopedContext {
    public:
      void *context;

      ScopedContext();
      ~ScopedContext();
  };

  //
  // ## Socket
  //
  // Much like the native `net` module, a ZMQStream socket (perhaps obviously) is really just a Duplex stream that
  // you can `connect`, `bind`, etc. just like a native ZMQ socket.
  //
  class Socket : public node::ObjectWrap {
    public:
      static v8::Persistent<v8::Function> constructor;

      //
      // ## Initialize
      //
      // Creates and populates the constructor Function and its prototype.
      //
      static void Initialize();

      //
      // ## InstallExports
      //
      // Exports the Socket class within the module `target`.
      //
      static void InstallExports(v8::Handle<v8::Object> target);

      //
      // ## Check
      //
      // To generate `'readable'` and `'drain'` events, we need to be polling our socket handles periodically. We
      // define that period to be once per event loop tick, and this is our libuv callback to handle that.
      //
      static void Check(uv_check_t* handle, int status);

      virtual ~Socket();

    protected:
      void *socket;
      uv_check_t handle;

      // We've fired the `'drain'` event, but have not called `write` yet.
      bool drain;
      // We've fired the `'readable'` event, but have not called `read` yet.
      bool readable;

      Socket(int type);

      //
      // ## Socket(options)
      //
      // Creates a new **options.type** ZMQ socket. Defaults to PAIR.
      //
      static v8::Handle<v8::Value> New(const v8::Arguments& args);

      //
      // ## Close `Close()`
      //
      // Closes the underlying ZMQ socket. _The stream should no longer be used!_
      //
      static v8::Handle<v8::Value> Close(const v8::Arguments& args);

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
      static v8::Handle<v8::Value> Read(const v8::Arguments& args);

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
      static v8::Handle<v8::Value> Write(const v8::Arguments& args);

      //
      // ## Connect `Connect(endpoint)`
      //
      // Connects the ZMQ socket to **endpoint**, expressed as a String.
      //
      // Connect is synchronous, and will throw an Error upon failure.
      //
      static v8::Handle<v8::Value> Connect(const v8::Arguments& args);

      //
      // ## Disconnect `Disconnect(endpoint)`
      //
      // Disconnects the ZMQ socket from **endpoint**, expressed as a String.
      //
      // Disconnect is synchronous, and will throw an Error upon failure.
      //
      static v8::Handle<v8::Value> Disconnect(const v8::Arguments& args);

      //
      // ## Bind `Bind(endpoint)`
      //
      // Binds the ZMQ socket to **endpoint**, expressed as a String.
      //
      // Bind is synchronous, and will throw an Error upon failure.
      //
      static v8::Handle<v8::Value> Bind(const v8::Arguments& args);

      //
      // ## Unbind `Unbind(endpoint)`
      //
      // Unbinds the ZMQ socket from **endpoint**, expressed as a String.
      //
      // Unbind is synchronous, and will throw an Error upon failure.
      //
      static v8::Handle<v8::Value> Unbind(const v8::Arguments& args);
  };

  //
  // ## Helpers
  //
  #define ZMQ_DEFINE_CONSTANT(target, name, constant)                       \
    (target)->Set(v8::String::NewSymbol(name),                              \
                  v8::Integer::New(constant),                               \
                  static_cast<v8::PropertyAttribute>(                       \
                      v8::ReadOnly|v8::DontDelete))
}

#endif
