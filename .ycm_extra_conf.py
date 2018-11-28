import os

def FlagsForFile(filename, **kwargs):
  return {
    'flags': [
      '-x', 'c',
      '-Wall', '-Wpointer-arith',
      '-fPIC',
      '-I.', '-I' + os.environ['HOME'] + '/include'
    ],
  }
