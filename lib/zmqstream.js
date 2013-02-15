/*jshint proto:true*/
var zmqstream = require('bindings')('zmqstream.node')
  , EventEmitter = require('events').EventEmitter

// We're using `__proto__` deliberately here to inherit a JS class to a C++ class.
// While we'd prefer to use `util.inherits`, that wipes out the already-completed prototype.
zmqstream.Socket.super_ = EventEmitter
zmqstream.Socket.prototype.__proto__ = new EventEmitter()

module.exports = zmqstream
