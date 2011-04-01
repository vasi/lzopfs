import os

from SConsUtil import *

# Parallel build
SetOption('num_jobs', os.sysconf('SC_NPROCESSORS_ONLN'))

env = Environment(
    LINKFLAGS = '-g',
    CPPFLAGS = '-Wall -g -O0')

conf = Configure(env, help = False, config_h = 'config.h',
    custom_tests = { 'CheckPkg': CheckPkg, 'CheckMac': CheckMac })

if not FindCXX(conf):
    print 'No compiler found.'
    Exit(1)

if not FindFUSE(conf):
    print 'FUSE not found.'
    Exit(1)

if not FindLib(conf, ['lzo2']):
    print 'LZO not found.'
    Exit(1)
if not FindLib(conf, ['lzma']):
    print 'LZMA not found.'
    Exit(1)
if not FindLib(conf, ['z']):
    print 'zlib not found.'
    Exit(1)

if not conf.CheckHeader('libkern/OSByteOrder.h'):
    if not conf.CheckHeader('endian.h'):
        print 'No endianness header found.'
        Exit(1)

env = conf.Finish()
env.Program('lzopfs', Glob('*.cc'))
