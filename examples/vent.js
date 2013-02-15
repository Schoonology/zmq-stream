var zmqstream = require('./lib/zmqstream')
  , BROKER_URL = 'ipc:///tmp/zmqtestbr'
  , start = Date.now()
  , count = parseInt(process.argv[2], 10) || 1000
  , type = process.argv[3] || 'DEALER'
  , stream = new zmqstream.Socket({
      type: zmqstream[zmqstream.Type[type]]
    })
  , sent = 0
  , id

console.log('Venting ' + count + ' messages with a ' + type + '(' + stream.type + ') socket.')

function done() {
  console.log('Sent:', sent)
  console.log('Rate:', sent / (Date.now() - start) * 1000)
  stream.close()
  clearInterval(id)
}

function send() {
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

stream.connect(BROKER_URL)

stream.on('drain', function () {
  console.log('DRAIN')
  send()
})
send()

id = setInterval(function () {})

module.exports = zmqstream
