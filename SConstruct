import os

from SConsUtil import *

# Parallel build
SetOption('num_jobs', os.sysconf('SC_NPROCESSORS_ONLN'))

opt = os.environ.get('OPT', '-O0')
env = Environment(
    LINKFLAGS = '-g',
    CPPFLAGS = '-Wall -g %s' % opt,
    ENV = { 'PATH' : os.environ['PATH'] })
env.Append(CPPFLAGS = ' ' + os.environ.get('CPPFLAGS', ''))

conf = Configure(env, help = True, config_h = 'config.h',
    custom_tests = { 'CheckPkg': CheckPkg, 'CheckMac': CheckMac })

if not FindCXX(conf):
    print('No compiler found.')
    Exit(1)

if not FindTR1(conf):
    print('No unordered_map implementation found.')
    Exit(1)

if not FindFUSE(conf):
    print('FUSE not found.')
    Exit(1)

compression_libs = [
    ('LZO', 'lzo2', 'lzo/lzo1x.h', 'HAVE_LZO'),
    ('LZMA', 'lzma', 'lzma.h', 'HAVE_LZMA'),
    ('zlib', 'z', 'zlib.h', 'HAVE_ZLIB'),
    ('bipz2', 'bz2', 'bzlib.h', 'HAVE_BZIP2'),
]
compression_found = False
for name, lib, header, define in compression_libs:
    if FindLib(conf, lib, header):
        conf.env.Append(CPPDEFINES = define)
        compression_found = True
    else:
        print('%s not found, skipping' % name)
if not compression_found:
    print('No compression library found.')
    Exit(1)

if not FindLib(conf, 'pthread', 'pthread.h'):
    print('pthreads not found.')
    Exit(1)

if not conf.CheckHeader('libkern/OSByteOrder.h'):
    if not conf.CheckHeader('endian.h'):
        print('No endianness header found.')
        Exit(1)

env = conf.Finish()
env.Program('lzopfs', Glob('*.cc'))
