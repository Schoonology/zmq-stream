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
      })

      it('should be able to bind without error', function () {

      })

      it('should be able to connect without error', function () {

      })

      it('should be able to unbind without error', function () {

      })

      it('should be able to disconnect without error', function () {

      })

      it('should be able to write messages without error', function () {
        this.socket.write([new Buffer('test')])
      })

      it('should be able to read messages without error', function () {
        this.socket.read(1)
      })
    })

    describe('REQ-REP', function () {
      it('should be able to write messages without error')
      it('should be able to read messages without error')
    })
  })
})
