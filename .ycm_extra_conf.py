import os

def FlagsForFile(filename, **kwargs):
  return {
    'flags': [
      '-x', 'c',
      '-Wall', '-Wpointer-arith',
      '-fPIC',
      '-D_GNU_SOURCE',
      '-I.', '-I' + os.environ['HOME'] + '/include'
    ],
  }
