var zmqstream = require('../lib/zmqstream')
  , iface = process.argv[3] || 'ipc:///tmp/zmqtestbr'
  // , start = null
  // , duration = 0
  , count = parseInt(process.argv[2], 10) || 1000
  , stream = new zmqstream.Socket({
      type: zmqstream.Type.ROUTER
    })
  , sent = 0
  , received = 0
  , queue = []

console.log('Expecting ' + count + ' messages from dealer.')
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

  if (!queue.length) {
    process.nextTick(send)
  }

  messages.forEach(function (envelope) {
    console.log(envelope.map(function (item) {
      return item.toString('hex')
    }))

    envelope[1] = new Buffer('pong')

    queue.push(envelope)
  })

  if (received >= count) {
    return
  }

  process.nextTick(recv)
}

function send() {
  if (!queue.length) {
    return
  }

  var full = !stream.write(queue.shift())

  if (full) {
    console.log('OUT EAGAIN:', sent)
  } else {
    sent++

    if (sent >= count) {
      done()
      return
    }

    process.nextTick(send)
  }
}

stream.bind(iface)

stream.on('drain', function () {
  console.log('DRAIN')
  send()
})
stream.on('readable', function () {
  console.log('READABLE')
  recv()
})
recv()
