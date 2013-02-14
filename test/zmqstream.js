/*global describe:true, it:true, before:true, after:true, beforeEach:true, afterEach:true */
var zmqstream = require('../lib/zmqstream')
  , expect = require('chai').expect

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

    describe('PAIR', function () {
      beforeEach(function () {
        var self = this

        self.socket = new zmqstream.Socket({
          type: zmqstream.Type.PAIR
        })
        self.endpoint = 'inproc:///tmp/zmqstreamtest' + Math.random().toString().slice(2, 6)
      })

      it('should be able to bind without error', function () {
        this.socket.bind(this.endpoint)
      })

      it('should be able to connect without error', function () {
        this.socket.connect(this.endpoint)
      })

      it('should be able to unbind without error', function () {
        this.socket.unbind(this.endpoint)
      })

      it('should be able to disconnect without error', function () {
        this.socket.disconnect(this.endpoint)
      })

      it('should be able to write messages without error', function () {
        this.socket.write([new Buffer('test')])
      })

      it('should be able to read messages without error', function () {
        this.socket.read(1)
      })

      // TODO: Multiple messages, not multiple frames.
      it('should be able to read the messages sent', function () {
        var a = new zmqstream.Socket()
          , b = new zmqstream.Socket()
          , message = [new Buffer('one'), new Buffer('two')]

        a.bind(this.endpoint)
        b.connect(this.endpoint)

        a.write(message)
        message = b.read()

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
    })

    describe('REQ-REP', function () {
      it('should be able to write messages without error')
      it('should be able to read messages without error')
    })
  })
})
