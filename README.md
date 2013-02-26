# ZMQStream

A set of stream-based Node bindings for ZeroMQ. The API is modeled after the [streams2](http://blog.nodejs.org/2012/12/20/streams2/) API.

## Differences from Streams2

Since [ZeroMQ is Not a Neutral Carrier](http://zguide.zeromq.org/page:all#-MQ-is-Not-a-Neutral-Carrier), the streams2 Duplex API ZMQStream uses is by _message_, not by byte. While the Duplex API is carried over in spirit, _do not_ try to read and write as if they're bytestreams, nor should you `pipe` a bytestream into or out of a ZMQStream Socket.

If desired, Transform streams could be used to replace the expected framing, translating the message stream into/out of a bytestream. When an implementation becomes available (it's considered to be outside the scope of this package), it'll be linked to here.

## Additional Concerns

 * There is a 1:1 relationship between Node processes and ZMQ contexts. Please submit an issue if this causes problems. Breaking this strict relationship will require discussion around specific use cases.

## Installation

Before you can install ZMQStream with NPM, you need to have the development source for ZeroMQ 3.2.x installed locally. This will be a platform-dependent task, but most platforms have tools to make this easier:

```bash
brew install zeromq --devel
yum install zeromq-devel
```

After that's ready, npm can be used as normal to install ZMQStream:

```bash
npm install zmq-stream
```

## Examples

```javascript
var zmqstream = require('zmq-stream')
  , socket

socket = zmqstream.createSocket({
  type: zmqstream.Type.ROUTER
})

socket.read()
socket.connect(process.env.URL)

socket.on('readable', function () {
  var messages = socket.read()

  console.log(messages[0])

  messages[0].pop()
  messages[0].push(new Buffer('pong'))

  socket.write(messages[0])
})
```

For more in-depth examples, please see the `examples` directory.

 * Vent/Sink - `node vent [COUNT] [TYPE]` , `node sink [COUNT] [TYPE]` - Vents `COUNT` messages toward sink over `TYPE` sockets. Defaults to 1000 messages with a PUSH vent socket and a PULL sink socket.
 * Router/Dealer - `node dealer [COUNT]` , `node router [COUNT]` - Sends `COUNT` messages from dealer to router, expecting `COUNT` responses in return with the same envelope. If `COUNT` is -1, it's deemed to be Infinity. Defaults to 1000 messages.
 * C Router/Dealer - `c/dealer [COUNT]` , `c/router [COUNT]` - Identical to Router/Dealer (except for -1 handling), but written using [CZMQ](http://czmq.zeromq.org/). Build using `make`, and be sure to [install CZMQ first](http://czmq.zeromq.org/page:get-the-software). Useful for portraying the inter-language compatability granted by ZeroMQ. Try running a C Router and a JS Dealer, and vice versa.

## API

### Constants

 * `zmqstream.Type` - Contains all legal `type` values. Example: `zmqstream.Type.XPUB`
 * `zmqstream.Option` - Contains all legal `option` values. Example: `zmqstream.Option.IDENTITY`

### createSocket `zmqstream.createSocket(options)` Also: `new Socket(options)`

Creates a new **options.type** Socket instance. Defaults to PAIR.

### Socket

A ZMQStream Socket is really (perhaps obviously) just a Duplex stream that you can `connect`, `bind`, etc. just like a native ZMQ socket.

#### Properties

 * `type` - The numerical type of the socket created.

#### close `socket.close()`

Closes and cleans up the underlying resources. Please ensure `close` is called once the Socket is no longer in use. _Do not_ call any other method after `close`.

#### set `socket.set(option, value)`

Sets **option** (e.g. `IDENTITY`) to **value** (e.g. `"ExampleClient"`), expressed as required by [ZMQ](http://api.zeromq.org/3-2:zmq-setsockopt) (some values should be Numbers or Booleans).

#### get `socket.get(option)`

Retrieves **option** as the format specified by [ZMQ](http://api.zeromq.org/3-2:zmq-getsockopt).

#### read `socket.read([size])`

Consumes a maximum of **size** messages of data. If **size** is undefined, the entire queue will be read and returned.

If there is no data to consume, or if there are fewer bytes in the internal buffer than the size argument, then `null` is returned, and a future `'readable'` event will be emitted when more is available.

Calling `stream.read(0)` is a no-op with no internal side effects, but can be used to test for Socket validity.

Returns an Array of Messages, which are in turn Arrays of Frames as Node Buffers.

NOTE: To reiterate, this Read returns a different amount in a different format than the builtin Duplex!

#### write `socket.write(message)`

Queues **message**, expressed as a Message (an Array of Buffers), to be transmitted over the wire at some time in the future.

Calling `stream.write([])` is a no-op with no internal side effects, but can be used for test for Socket validity.

Returns `true` if **message** was queued successfully, or `false` if the buffer is full (see [ZMQ_DONTWAIT/EAGAIN](http://api.zeromq.org/3-2:zmq-send)). If the buffer is full, a `'drain'` event will be emitted when space is again available for sending.

NOTE: Unlike the builtin Duplex class, a return value of `false` indicates the write was _unsuccessful_, and will need to be tried again in the future.

#### connect `socket.connect(endpoint)`

Synchronously connects to **endpoint**, expressed as a String, throwing an Error upon failure.

#### disconnect `socket.disconnect(endpoint)`

Synchronously disconnects from **endpoint**, expressed as a String, throwing an Error upon failure.

#### bind `socket.bind(endpoint)`

Synchronously binds to **endpoint**, expressed as a String, throwing an Error upon failure.

#### unbind `socket.unbind(endpoint)`

Synchronously unbinds from **endpoint**, expressed as a String, throwing an Error upon failure.

## Alternatives & Comparisons

 * [zmq](http://npmjs.org/package/zmq) - `zmq` has a much "nicer" per-message `send` method, one frame per argument. In addition, all incoming messages are broadcast as a `"message"` event on the socket, also with one frame per argument. That said, `zmq` does not have special treatment for HMM/EAGAIN issues, and has a more limited throughput (on the machine used to develop zmq-stream, zmq-stream hit ~140k msg/s where zmq hit ~4k).

## License

Copyright (C) 2013 Michael Schoonmaker (michael.r.schoonmaker@gmail.com)

This project is free software released under the MIT/X11 license:

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
