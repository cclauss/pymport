{
  'variables': {
    'enable_asan%': 'false',
    'enable_coverage%': 'false',
    'builtin_python%': 'false',
    'binding_dir': '<!(node -e "console.log(path.dirname(require(\'@mapbox/node-pre-gyp\').find(\'package.json\')))")',
    'bin_path': '<!(node -e "console.log(path.resolve(path.dirname(require(\'@mapbox/node-pre-gyp\').find(\'package.json\'))))")',
  },
  'targets': [
    {
      'target_name': 'pymport',
      'sources': [ 
        'src/main.cc',
        'src/pyobj.cc',
        'src/call.cc',
        'src/fromjs.cc',
        'src/tojs.cc',
        'src/objstore.cc'
      ],
      'include_dirs': [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      'defines': [
        'NAPI_EXPERIMENTAL'
        'NODE_ADDON_API_DISABLE_DEPRECATED',
        'NAPI_VERSION=6'
      ],
      'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
      'conditions': [
        ['enable_asan == "true"', {
          'cflags_cc': [ '-fsanitize=address' ],
          'ldflags' : [ '-fsanitize=address' ]
        }],
        ['enable_coverage == "true"', {
          'cflags_cc': [ '-fprofile-arcs', '-ftest-coverage' ],
          'ldflags' : [ '-lgcov', '--coverage' ]
        }],
        ['OS == "win"', {
          'msvs_settings': {
            'VCCLCompilerTool': { 'ExceptionHandling': 1 },
          },
          'conditions': [
            ['builtin_python == "false"', {
              'include_dirs': [ '<!(python -c "import os, sys; print(os.path.dirname(sys.executable))")/include' ],
              'msvs_settings': {
                'VCLinkerTool': {
                  'AdditionalLibraryDirectories': '<!(python -c "import os, sys; print(os.path.dirname(sys.executable))")/libs'
                }
              },
            }],
            ['builtin_python == "true"', {
              'dependencies': [ 'builtin_python' ],
              'include_dirs': [ 'build\Python-3.10.8\Include', 'build\Python-3.10.8\PC' ],
              'msvs_settings': {
                'VCLinkerTool': {
                  'AdditionalLibraryDirectories': '<(bin_path)'
                }
              },
            }]]
        }],
        ['OS != "win"', {
          'conditions': [
            ['builtin_python == "false"', {
              'cflags': [ '<!@(pkg-config --cflags python3-embed)' ],
              'libraries': [ '<!@(pkg-config --libs python3-embed)' ]
            }],
            ['builtin_python == "true"', {
              'dependencies': [ 'builtin_python' ],
              'include_dirs': [ '<(bin_path)/include/python3.10' ],
              'libraries': [ '-L <(bin_path)/lib/ -lpython3.10' ],
              'ldflags': [ '-Wl,-z,origin', '-Wl,-rpath,\'$$ORIGIN/lib\'' ]
            }]
          ],
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
            'CLANG_CXX_LIBRARY': 'libc++',
            'MACOSX_DEPLOYMENT_TARGET': '10.7',
            'OTHER_CFLAGS': [ '<!@(pkg-config --cflags python3-embed)' ],
            'OTHER_LDFLAGS': [ '-Wl,-rpath,@loader_path/lib' ]
          },
          'cflags!': [ '-fno-exceptions' ],
          'cflags_cc!': [ '-fno-exceptions' ],
        }]
      ]
    },
    {
      'target_name': 'action_after_build',
      'type': 'none',
      'dependencies': [ '<(module_name)' ],
      'copies': [
        {
          'files': [
            '<(PRODUCT_DIR)/pymport.node',
          ],
          'destination': '<(module_path)'
        }
      ]
    }
  ],
  'conditions': [
    ['builtin_python == "true" and OS == "linux"', {
      'targets': [{
        'target_name': 'builtin_python',
        'type': 'none',
        'actions': [{
          'action_name': 'Python',
          'inputs': [ './build_python.sh' ],
          'outputs': [ '<(bin_path)/lib/libpython3.10.so' ],
          'action': [ 'sh', 'build_python.sh', '<(bin_path)' ]
        }]
      }]
    }],
    ['builtin_python == "true" and OS == "mac"', {
      'targets': [{
        'target_name': 'builtin_python',
        'type': 'none',
        'actions': [{
          'action_name': 'Python',
          'inputs': [ './build_python.sh' ],
          'outputs': [ '<(bin_path)/lib/libpython3.10.dylib' ],
          'action': [ 'sh', 'build_python.sh', '<(bin_path)' ]
        }]
      }]
    }],
    ['builtin_python == "true" and OS == "win"', {
      'targets': [{
        'target_name': 'builtin_python',
        'type': 'none',
        'actions': [{
          'action_name': 'Python',
          'inputs': [ './build_python.bat' ],
          'outputs': [ '<(bin_path)/Python310.lib' ],
          'action': [ '<(module_root_dir)/build_python.bat', '<(bin_path)' ]
        }]
      }]
    }]
  ]
}
