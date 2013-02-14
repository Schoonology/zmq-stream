var zmqstream = require('./lib/zmqstream')
  , BROKER_URL = 'ipc:///tmp/zmqtestbr'

if (require.main === module) {
  var start = Date.now()
    , count = parseInt(process.argv[2], 10) || 1000
    , stream = new zmqstream.Socket(zmqstream.DEALER)

  stream.connect(BROKER_URL)

  for (var i = 0; i < count; i++) {
    if (!stream.write([new Buffer(''), new Buffer('Message.')])) {
      console.error('EAGAIN:', i)
      break
    }
  }

  console.log('Rate:', i / (Date.now() - start) * 1000)
  stream.close()
}

module.exports = zmqstream
