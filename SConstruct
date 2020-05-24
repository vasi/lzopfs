import os

from SConsUtil import *

# Parallel build
SetOption('num_jobs', os.sysconf('SC_NPROCESSORS_ONLN'))

opt = os.environ.get('OPT', '-O0')
env = Environment(
    LINKFLAGS = '-g',
    CPPFLAGS = '-Wall -g %s' % opt,
    ENV = { 'PATH' : os.environ['PATH'] })

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

if not FindLib(conf, 'lzo2', 'lzo/lzo1x.h'):
    print('LZO not found.')
    Exit(1)
if not FindLib(conf, 'lzma', 'lzma.h'):
    print('LZMA not found.')
    Exit(1)
if not FindLib(conf, 'z', 'zlib.h'):
    print('zlib not found.')
    Exit(1)
if not FindLib(conf, 'bz2', 'bzlib.h'):
    print('bzip2 not found.')
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
