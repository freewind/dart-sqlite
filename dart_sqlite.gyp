# Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
# for details. All rights reserved. Use of this source code is governed by a
# BSD-style license that can be found in the LICENSE file.

{
  'target_defaults': {
    'default_configuration': 'Release',
    'configurations': {
      'Release': {
        'cflags': [ '-O2' ]
      },
      'Debug': {
        'cflags': [ '-g', '-O0' ]
      },
    }
  },
  'targets': [
    {
      'target_name': 'dart_sqlite',
      'type': 'shared_library',
      'product_dir': './lib',
      'include_dirs': [
        '$(DART_SDK)',
      ],
      'sources': [
        'src/dart_sqlite.cc'
      ],
      'defines': [
        'DART_SHARED_LIB',
      ],
      'link_settings': {
        'libraries': [
          '-lsqlite3'
        ]
      },
      'conditions': [
        ['OS=="linux"', {
          'cflags': [
            '-fPIC'
          ]
        }],
      ],
    },
  ],
}
