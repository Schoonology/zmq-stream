var zmqstream = require('../lib/zmqstream')
  , iface = process.argv[3] || 'ipc:///tmp/zmqtestbr'
  // , start = null
  // , duration = 0
  , count = parseInt(process.argv[2], 10) || 1000
  , stream = new zmqstream.Socket({
      type: zmqstream.Type.DEALER
    })
  , sent = 0
  , received = 0

console.log('Pinging router with ' + count + ' messages.')
console.log('PID:', process.pid)

function done() {
  console.log('Sent:', sent)
  console.log('received:', received)
  // console.log('Rate:', sent / (duration) * 1000)
  stream.close()
}

function recv() {
  var messages = stream.read(10)

  if (!messages) {
    console.log('IN EAGAIN:', received)
    return
  }

  console.log('Got:', messages.length)

  received += messages.length

  if (received >= count) {
    done()
    return
  }

  process.nextTick(recv)
}

function send() {
  if (sent === count) {
    return
  }

  // if (start == null) {
  //   start = Date.now()
  // }

  var full = !stream.write([new Buffer(''), new Buffer('ping')])

  if (full) {
    console.log('OUT EAGAIN:', sent)
  } else {
    sent++
    process.nextTick(send)
  }
}

stream.connect(iface)

stream.on('drain', function () {
  console.log('DRAIN')
  send()
})
stream.on('readable', function () {
  console.log('READABLE')
  recv()
})
send()
