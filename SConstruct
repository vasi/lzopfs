import os
SetOption('num_jobs', os.sysconf('SC_NPROCESSORS_ONLN'))

Program('lzopfs', Glob('*.cc'),
    CXX = 'clang++',
    CPPPATH = '/opt/local/include',
    CPPFLAGS = '-D__FreeBSD__=10 -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 ' +
        '-D__DARWIN_64_BIT_INO_T=1 -Wall -g',
    LINKFLAGS = '-g',
    LIBPATH = '/opt/local/lib',
    LIBS = Split('lzo2 fuse_ino64'))
