//
// # Sink
//
// TODO: Description.
//
var zmqstream = require('../lib/zmqstream')

//
// ## Sink `Sink(obj)`
//
// Creates a new instance of Sink with the following options:
//
function Sink(obj) {
  if (!(this instanceof Sink)) {
    return new Sink(obj)
  }

  obj = obj || {}

  this.startDate = null
  this.count = obj.count || 1000
  this.iface = obj.iface || 'ipc:///tmp/zmqtestbr'
  this.type = obj.type || 'PULL'
  this.received = 0

  this.stream = new zmqstream.Socket({
    type: zmqstream.Type[this.type]
  })
}

//
// ## start `start()`
//
// Starts the Sink.
//
Sink.prototype.start = start
function start() {
  var self = this

  console.log('Sinking ' + self.count + ' messages with a ' + self.type + '(' + self.stream.type + ') socket.')
  console.log('PID:', process.pid)

  self.stream.bind(self.iface)

  self.stream.on('readable', function () {
    console.log('READABLE')
    self.recv()
  })
  self.recv()
}

//
// ## stop `stop()`
//
// Stops the Sink.
//
Sink.prototype.stop = stop
function stop() {
  var self = this

  console.log('Received:', self.received)
  console.log('Rate:', self.received / (Date.now() - self.startDate) * 1000)
  self.stream.close()
}

//
// ## recv `recv()`
//
// Receives as many messages as possible, up to 100.
//
Sink.prototype.recv = recv
function recv() {
  var self = this
    , messages = self.stream.read(100)

  if (messages) {
    if (self.startDate == null) {
      self.startDate = Date.now()
    }

    console.log('Got:', messages.length)
    self.received += messages.length

    if (self.received >= self.count) {
      self.stop()
    } else {
      process.nextTick(function () {
        self.recv()
      })
    }
  } else {
    console.log('EAGAIN:', self.received)
  }
}

module.exports = Sink

//
// ## Running
//
// If `sink` is required directly, we want to start a new Sink, actively receiving `argv.count` messages.
//
if (require.main === module) {
  var sink = new Sink({
    count: parseInt(process.argv[2], 10),
    type: process.argv[3],
    iface: process.argv[4]
  })

  sink.start()
}
