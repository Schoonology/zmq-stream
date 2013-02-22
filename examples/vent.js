var zmqstream = require('../lib/zmqstream')
  , iface = process.argv[4] || 'ipc:///tmp/zmqtestbr'
  , start = null
  , count = parseInt(process.argv[2], 10) || 1000
  , type = process.argv[3] || 'DEALER'
  , stream = new zmqstream.Socket({
      type: zmqstream.Type[type]
    })
  , sent = 0

console.log('Venting ' + count + ' messages with a ' + type + '(' + stream.type + ') socket.')
console.log('PID:', process.pid)

function done() {
  console.log('Sent:', sent)
  console.log('Rate:', sent / (Date.now() - start) * 1000)
  stream.close()
}

function send() {
  if (start == null) {
    start = Date.now()
  }

  if (sent === count) {
    done()
    return
  }

  var full = !stream.write([new Buffer(''), new Buffer('Message.')])

  if (full) {
    console.log('EAGAIN:', sent)
  } else {
    sent++
    send()
  }
}

stream.connect(iface)

stream.on('drain', function () {
  console.log('DRAIN')
  send()
})
send()

module.exports = zmqstream
