/*global describe:true, it:true, before:true, after:true, beforeEach:true, afterEach:true */
var zmqstream = require('../lib/zmqstream')
  , expect = require('chai').expect

function getInprocEndpoint() {
  return 'inproc://zmqstreamtest' + Math.random().toString().slice(2, 6)
}

describe('ZMQStream', function () {
  it('should exist', function () {
    expect(zmqstream).to.exist
    expect(zmqstream).to.be.an('object')
  })

  before(function () {
    var self = this

    self.sockets = []
  })

  afterEach(function () {
    var self = this

    self.sockets.forEach(function (socket) {
      socket.close()
    })
    self.sockets = []
  })

  describe('Constants', function () {
    it('should exist', function () {
      expect(zmqstream.Type).to.exist
      expect(zmqstream.Type).to.be.an('object')
    })

    it('should export all socket types', function () {
      expect(zmqstream.Type.REQ, 'REQ').to.exist
      expect(zmqstream.Type.REP, 'REP').to.exist
      expect(zmqstream.Type.DEALER, 'DEALER').to.exist
      expect(zmqstream.Type.ROUTER, 'ROUTER').to.exist
      expect(zmqstream.Type.PUB, 'PUB').to.exist
      expect(zmqstream.Type.SUB, 'SUB').to.exist
      expect(zmqstream.Type.XPUB, 'XPUB').to.exist
      expect(zmqstream.Type.XSUB, 'XSUB').to.exist
      expect(zmqstream.Type.PUSH, 'PUSH').to.exist
      expect(zmqstream.Type.PULL, 'PULL').to.exist
      expect(zmqstream.Type.PAIR, 'PAIR').to.exist
    })
  })

  describe('Socket', function () {
    it('should exist', function () {
      expect(zmqstream.Socket).to.exist
      expect(zmqstream.Socket).to.be.a('function')
    })

    it('should default to PAIR if no options are provided', function () {
      var socket = new zmqstream.Socket()

      expect(socket.type).to.equal(zmqstream.Type.PAIR)
    })

    it('should default to PAIR if no type is provided', function () {
      var socket = new zmqstream.Socket({})

      expect(socket.type).to.equal(zmqstream.Type.PAIR)
    })

    it('should construct an object be the correct type', function () {
      var socket = zmqstream.Socket()

      expect(socket).to.exist
      expect(socket).to.be.an('object')
      expect(socket).to.be.an.instanceof(zmqstream.Socket)
    })

    it('should be constructed without error when called without new', function () {
      var socket = zmqstream.Socket()

      expect(socket).to.exist
      expect(socket).to.be.an.instanceof(zmqstream.Socket)
      expect(socket.type).to.equal(zmqstream.Type.PAIR)
    })

    describe('write', function () {
      beforeEach(function () {
        var self = this

        self.socket = new zmqstream.Socket()
      })

      it('should be callable without error with 0 frames', function () {
        this.socket.write([])
      })

      it('should be callable without error with 1 frame', function () {
        this.socket.write([new Buffer('test')])
      })

      it('should be callable without error with n frames', function () {
        this.socket.write([new Buffer('one'), new Buffer('two'), new Buffer('three')])
      })

      it('should throw if the Socket is closed', function () {
        var socket = new zmqstream.Socket({
          type: zmqstream.Type.REQ
        })

        socket.close()

        expect(function () {
          socket.write([])
        }).to.throw('Socket is closed')
      })
    })

    describe('read', function () {
      beforeEach(function () {
        var self = this

        self.socket = new zmqstream.Socket()
        self.endpoint = getInprocEndpoint()
      })

      it('should be callable without error with 0 frames', function () {
        this.socket.read(0)
      })

      it('should be callable without error with 1 frame', function () {
        this.socket.read(1)
      })

      it('should be callable without error with n frames', function () {
        this.socket.read(3)
      })

      it('should receive the frames of a single message in order', function () {
        var sender = new zmqstream.Socket()
          , message = [new Buffer('one'), new Buffer('two')]

        sender.bind(this.endpoint)
        this.socket.connect(this.endpoint)

        sender.write(message)
        message = this.socket.read()

        expect(message).to.exist
        expect(message).to.be.an.instanceof(Array)
        expect(message).to.have.length(1)
        expect(message[0]).to.be.an.instanceof(Array)
        expect(message[0]).to.have.length(2)
        expect(message[0][0]).to.be.an.instanceof(Buffer)
        expect(message[0][0]).to.have.length(3)
        expect(message[0][0].toString()).to.equal('one')
        expect(message[0][1]).to.be.an.instanceof(Buffer)
        expect(message[0][1]).to.have.length(3)
        expect(message[0][1].toString()).to.equal('two')
      })

      it('should receive multiple messages in order', function () {
        var sender = new zmqstream.Socket()
          , messages = [
              [new Buffer('one'), new Buffer('two')],
              [new Buffer('three'), new Buffer('four'), new Buffer('five')]
            ]

        sender.bind(this.endpoint)
        this.socket.connect(this.endpoint)

        sender.write(messages[0])
        sender.write(messages[1])
        messages = this.socket.read()

        // Two messages
        expect(messages).to.exist
        expect(messages).to.be.an.instanceof(Array)
        expect(messages).to.have.length(2)

        // Two frames (one, two) in the first message
        expect(messages[0]).to.be.an.instanceof(Array)
        expect(messages[0]).to.have.length(2)
        expect(messages[0][0]).to.be.an.instanceof(Buffer)
        expect(messages[0][0].toString()).to.equal('one')
        expect(messages[0][1]).to.be.an.instanceof(Buffer)
        expect(messages[0][1].toString()).to.equal('two')

        // Three frames (three, four, fice) in the second message
        expect(messages[1]).to.be.an.instanceof(Array)
        expect(messages[1]).to.have.length(3)
        expect(messages[1][0]).to.be.an.instanceof(Buffer)
        expect(messages[1][0].toString()).to.equal('three')
        expect(messages[1][1]).to.be.an.instanceof(Buffer)
        expect(messages[1][1].toString()).to.equal('four')
        expect(messages[1][2]).to.be.an.instanceof(Buffer)
        expect(messages[1][2].toString()).to.equal('five')
      })

      it('should throw if the Socket is closed', function () {
        var socket = new zmqstream.Socket({
          type: zmqstream.Type.REQ
        })

        socket.close()

        expect(function () {
          socket.read(0)
        }).to.throw('Socket is closed')
      })
    })

    describe('bind', function () {
      beforeEach(function () {
        this.socket = new zmqstream.Socket()
        this.endpoint = getInprocEndpoint()
      })

      it('should be callable without error', function () {
        this.socket.bind(this.endpoint)
      })

      it('should throw if no endpoint is provided', function () {
        var self = this

        expect(function () {
          self.socket.bind()
        }).to.throw('No endpoint')
      })
    })

    describe('connect', function () {
      beforeEach(function () {
        this.socket = new zmqstream.Socket()
        this.endpoint = getInprocEndpoint()
      })

      it('should be callable without error', function () {
        this.socket.connect(this.endpoint)
      })

      it('should throw if no endpoint is provided', function () {
        var self = this

        expect(function () {
          self.socket.connect()
        }).to.throw('No endpoint')
      })
    })

    describe('unbind', function () {
      beforeEach(function () {
        this.socket = new zmqstream.Socket()
        this.endpoint = getInprocEndpoint()
      })

      it('should be callable without error', function () {
        this.socket.unbind(this.endpoint)
      })

      it('should throw if no endpoint is provided', function () {
        var self = this

        expect(function () {
          self.socket.unbind()
        }).to.throw('No endpoint')
      })
    })

    describe('disconnect', function () {
      beforeEach(function () {
        this.socket = new zmqstream.Socket()
        this.endpoint = getInprocEndpoint()
      })

      it('should be callable without error', function () {
        this.socket.disconnect(this.endpoint)
      })

      it('should throw if no endpoint is provided', function () {
        var self = this

        expect(function () {
          self.socket.disconnect()
        }).to.throw('No endpoint')
      })
    })

    describe('REQ-REP', function () {
      it('should be able to write messages without error')
      it('should be able to read messages without error')
    })
  })
})
