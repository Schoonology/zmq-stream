//
// # Vent
//
// TODO: Description.
//
var zmqstream = require('../lib/zmqstream')

//
// ## Vent `Vent(obj)`
//
// Creates a new instance of Vent with the following options:
//
function Vent(obj) {
  if (!(this instanceof Vent)) {
    return new Vent(obj)
  }

  obj = obj || {}

  this.startDate = null
  this.count = obj.count || 1000
  this.iface = obj.iface || 'ipc:///tmp/zmqtestbr'
  this.type = obj.type || 'PUSH'
  this.sent = 0

  this.stream = new zmqstream.Socket({
    type: zmqstream.Type[this.type]
  })
}

//
// ## start `start()`
//
// Starts the Vent.
//
Vent.prototype.start = start
function start() {
  var self = this

  console.log('Venting ' + self.count + ' messages with a ' + self.type + '(' + self.stream.type + ') socket.')
  console.log('PID:', process.pid)

  self.stream.connect(self.iface)

  self.stream.on('drain', function () {
    console.log('DRAIN')
    self.send()
  })
  self.send()
}

//
// ## stop `stop()`
//
// Stops the Vent.
//
Vent.prototype.stop = stop
function stop() {
  var self = this

  console.log('Sent:', self.sent)
  console.log('Rate:', self.sent / (Date.now() - self.startDate) * 1000)
  self.stream.close()
}

//
// ## send `send()`
//
// Recursively sends as many messages as possible, up to 100 messages per event loop.
//
Vent.prototype.send = send
function send() {
  var self = this

  if (self.startDate == null) {
    self.startDate = Date.now()
  }

  if (self.sent === self.count) {
    self.stop()
    return
  }

  var full = !self.stream.write([new Buffer(''), new Buffer('Message.')])

  if (full) {
    console.log('EAGAIN:', self.sent)
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

module.exports = Vent

//
// ## Running
//
// If `vent` is required directly, we want to start a new Vent, actively sending `argv.count` messages.
//
if (require.main === module) {
  var vent = new Vent({
    count: parseInt(process.argv[2], 10),
    type: process.argv[3],
    iface: process.argv[4]
  })

  vent.start()
}
