//
// # Router
//
// TODO: Description.
//
var zmqstream = require('../lib/zmqstream')

//
// ## Router `Router(obj)`
//
// Creates a new instance of Router with the following options:
//
function Router(obj) {
  if (!(this instanceof Router)) {
    return new Router(obj)
  }

  obj = obj || {}

  this.count = obj.count || 1000
  this.iface = obj.iface || 'ipc:///tmp/zmqtestbr'
  this.sent = 0
  this.received = 0
  this.queue = []

  this.stream = new zmqstream.Socket({
    type: zmqstream.Type.ROUTER
  })
}

//
// ## start `start()`
//
// Starts the Router.
//
Router.prototype.start = start
function start() {
  var self = this

  console.log('Expecting ' + self.count + ' messages from dealer.')
  console.log('PID:', process.pid)

  self.stream.bind(self.iface)

  self.stream.on('drain', function () {
    console.log('DRAIN')
    self.send()
  })
  self.stream.on('readable', function () {
    console.log('READABLE')
    self.recv()
  })
  self.recv()
}

//
// ## stop `stop()`
//
// Stops the Router.
//
Router.prototype.stop = stop
function stop() {
  var self = this

  console.log('Received:', self.received)
  console.log('Sent:', self.sent)
  self.stream.close()
}

//
// ## recv `recv()`
//
// Receives as many messages as possible, up to 100.
//
Router.prototype.recv = recv
function recv() {
  var self = this
    , messages = self.stream.read(100)

  if (!messages) {
    console.log('IN EAGAIN:', self.received)
    return
  }

  console.log('Got:', messages.length)
  self.received += messages.length

  if (!self.queue.length) {
    process.nextTick(function () {
      self.send()
    })
  }

  messages.forEach(function (envelope) {
    // console.log(envelope.map(function (item) {
    //   return item.toString('hex')
    // }))

    envelope[1] = new Buffer('pong')

    self.queue.push(envelope)
  })

  if (self.received >= self.count) {
    return
  }

  process.nextTick(function () {
    self.recv()
  })
}

//
// ## send `send()`
//
// Recursively sends responses as fast as possible, up to one per event loop.
//
Router.prototype.send = send
function send() {
  var self = this

  if (!self.queue.length) {
    return
  }

  var full = !self.stream.write(self.queue.shift())

  if (full) {
    console.log('OUT EAGAIN:', self.sent)
  } else {
    self.sent++

    if (self.sent >= self.count) {
      self.stop()
      return
    }

    if (self.sent % 100 === 0) {
      process.nextTick(function () {
        self.send()
      })
    } else {
      self.send()
    }
  }
}

module.exports = Router

//
// ## Running
//
// If `router` is required directly, we want to start a new Router, actively receiving `argv.count` messages.
//
if (require.main === module) {
  var router = new Router({
    count: parseInt(process.argv[2], 10),
    iface: process.argv[3]
  })

  router.start()
}
