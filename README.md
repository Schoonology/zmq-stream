# ZMQStream

Since [ZeroMQ is Not a Neutral Carrier](http://zguide.zeromq.org/page:all#-MQ-is-Not-a-Neutral-Carrier), the streams2 Duplex API ZMQStream uses is by _message_, not by byte. While the Duplex API is carried over in spirit, _do not_ try to read and write as if they're bytestreams, nor should you `pipe` a bytestream into or out of a ZMQStream Socket.
