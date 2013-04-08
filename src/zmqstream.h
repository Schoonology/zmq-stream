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
      // A `uv_poll_cb` registered to facilitate generating `'readable'` and `'drain'` events.
      //
      static void Check(uv_poll_t* handle, int status, int events);

      //
      // ## Check
      //
      // A `uv_idle_cb` registered to facilitate generating `'readable'` and `'drain'` events.
      //
      static void Check(uv_idle_t* handle, int status);

      //
      // ## Check
      //
      // The actual workhorse responsible for checking ZMQ_EVENTS, firing `'readable'` and `'drain'` as appropriate.
      //
      static void Check(Socket *self);

      virtual ~Socket();

    protected:
      // The actual ZeroMQ socket instance.
      void *socket;

      // Since "after calling zmq_send the socket may become readable (and vice versa) without triggering a read event
      // on the file descriptor", and that same file descriptor is signaled in an edge-triggered fashion by ZeroMQ, we
      // need a combination of approaches to integrate it with Libuv:
      //
      // A pair of uv_poll handles are responsible for picking up on readability/writability tests out of band with send
      // and recv calls.
      uv_poll_t readableHandle;
      uv_poll_t writableHandle;
      // A uv_idle handle is responsible for queueing Check calls to be called "soon".
      uv_idle_t idleHandle;
      // A flag that is true when the application should expect a "drain" event.
      bool shouldDrain;
      // A flag that is true when the application should expect a "readable" event.
      bool shouldReadable;

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
      // ## SetOption `SetOption(option, value)`
      //
      // Sets a ZMQ-specific option on the underlying ZMQ socket.
      //
      static v8::Handle<v8::Value> SetOption(const v8::Arguments& args);

      //
      // ## GetOption `GetOption(option)`
      //
      // Retrieves a ZMQ-specific option from the underlying ZMQ socket.
      //
      static v8::Handle<v8::Value> GetOption(const v8::Arguments& args);

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
}

#endif
