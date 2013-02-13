{
  'targets': [
    {
      'target_name': 'zmqstream',
      'sources': [ 'src/zmqstream.cc' ],
      # TODO: Build for other platforms.
      'link_settings': {
        'libraries': [
          '-lzmq'
        ]
      }
    }
  ]
}
