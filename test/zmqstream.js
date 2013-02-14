/*global describe:true, it:true, before:true, after:true, beforeEach:true, afterEach:true */
var zmqstream = require('../lib/zmqstream')
  , expect = require('chai').expect

describe('ZMQStream', function () {
  describe('Constants', function () {
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
})
