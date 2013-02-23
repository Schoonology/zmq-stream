//
// # Dealer
//
// TODO: Description.
//
var zmqstream = require('../lib/zmqstream')

//
// ## Dealer `Dealer(obj)`
//
// Creates a new instance of Dealer with the following options:
//
function Dealer(obj) {
  if (!(this instanceof Dealer)) {
    return new Dealer(obj)
  }

  obj = obj || {}

  this.count = obj.count || 1000
  this.iface = obj.iface || 'ipc:///tmp/zmqtestbr'
  this.sent = 0
  this.received = 0
  this.queue = []

  this.stream = new zmqstream.Socket({
    type: zmqstream.Type.DEALER
  })
  this.stream.set(zmqstream.Option.IDENTITY, 'ExampleDealer')
}

//
// ## start `start()`
//
// Starts the Dealer.
//
Dealer.prototype.start = start
function start() {
  var self = this

  console.log('Pinging router with ' + self.count + ' messages.')
  console.log('PID:', process.pid)

  self.stream.connect(self.iface)

  self.stream.on('drain', function () {
    console.log('DRAIN')
    self.send()
  })
  self.stream.on('readable', function () {
    console.log('READABLE')
    self.recv()
  })
  self.send()
}

//
// ## stop `stop()`
//
// Stops the Dealer.
//
Dealer.prototype.stop = stop
function stop() {
  var self = this

  console.log('Sent:', self.sent)
  console.log('Received:', self.received)
  self.stream.close()
}

//
// ## send `send()`
//
// Recursively sends responses as fast as possible, up to one per event loop.
//
Dealer.prototype.send = send
function send() {
  var self = this

  if (self.sent === self.count) {
    return
  }

  var full = !self.stream.write([new Buffer(''), new Buffer('ping:' + self.sent)])

  if (full) {
    console.log('OUT EAGAIN:', self.sent)
  } else {
    self.sent++

    if (self.sent % 100 === 0) {
      process.nextTick(function () {
        self.send()
      })
    } else {
      self.send()
    }
  }
}

//
// ## recv `recv()`
//
// Receives as many messages as possible, up to 100.
//
Dealer.prototype.recv = recv
function recv() {
  var self = this
    , messages = self.stream.read(100)

  if (!messages) {
    console.log('IN EAGAIN:', self.received)
    return
  }

  console.log('Got:', messages.length)
  self.received += messages.length

  if (self.received >= self.count) {
    self.stop()
    return
  }

  process.nextTick(function () {
    self.recv()
  })
}

module.exports = Dealer

//
// ## Running
//
// If `dealer` is required directly, we want to start a new Dealer, actively receiving `argv.count` messages.
//
if (require.main === module) {
  var dealer = new Dealer({
    count: parseInt(process.argv[2], 10),
    iface: process.argv[3]
  })

  dealer.start()
}
