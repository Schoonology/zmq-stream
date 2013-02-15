var zmqstream = require('../lib/zmqstream')
  , BROKER_URL = process.argv[4] || 'ipc:///tmp/zmqtestbr'
  , start = null
  , count = parseInt(process.argv[2], 10) || 1000
  , type = process.argv[3] || 'ROUTER'
  , stream = new zmqstream.Socket({
      type: zmqstream.Type[type]
    })
  , received = 0

console.log('Sinking ' + count + ' messages with a ' + type + '(' + stream.type + ') socket.')

function done() {
  console.log('Received:', received)
  console.log('Rate:', received / (Date.now() - start) * 1000)
  stream.close()
}

function recv() {
  var messages = stream.read(100)

  if (messages) {
    if (start == null) {
      start = Date.now()
    }

    console.log('Got:', messages.length)
    received += messages.length

    if (received >= count) {
      done()
    } else {
      process.nextTick(recv)
    }
  } else {
    console.log('EAGAIN:', received)
  }
}

stream.bind(BROKER_URL)

stream.on('readable', function () {
  console.log('READABLE')
  recv()
})

recv()

module.exports = zmqstream
