import os
import platform

from SCons.Util import WhereIs

# Does pkg-config know about a package?
def CheckPkg(ctx, pkg):
    ctx.Message('Checking if pkg-config can find %s...' % pkg)
    ret = ctx.TryAction('pkg-config --exists %s' % pkg)[0]
    if ret:
        ctx.env.ParseConfig('pkg-config --cflags --libs %s' % pkg)
    ctx.Result(ret)
    return ret

# Are we on a Mac?
mac = None
def IsMac(conf):
    global mac
    if mac == None:
        mac = conf.CheckMac()
    return mac

def CheckMac(ctx):
    ctx.Message('Checking if this is Mac OS X...')
    ret = (platform.system() == 'Darwin')
    ctx.Result(ret)
    return ret

# Find a lib, checking for any Mac package managers in strange prefixes
pkgmans = {}
def FindLib(conf, lib, hdr, managers = ['port', 'fink']):
    global pkgmans
    if conf.CheckLib(lib) and conf.CheckHeader(hdr):
        return True
    if not IsMac(conf):
        return False

    conf.env.Append(LIBPATH = []) # ensure it exists
    conf.env.Append(CPPPATH = []) # ensure it exists
    for pkgman in managers:
        if pkgman in pkgmans:
            continue # already have it
        loc = WhereIs(pkgman)
        if not loc:
            continue
        loc = os.path.realpath(loc)
        bin = 'bin/%s' % pkgman
        pre, sep, post = loc.partition(bin)
        if sep != bin or post != '':
            continue

        orig_libpath = conf.env['LIBPATH']
        orig_cpppath = conf.env['CPPPATH']
        conf.env.Append(LIBPATH = [pre + "/lib"])
        conf.env.Append(CPPPATH = [pre + "/include"])
        print("Try package manager '%s'..." % pkgman)
        if conf.CheckLib(lib) and conf.CheckHeader(hdr):
            pkgmans[pkgman] = True
            return True
        conf.env.Replace(LIBPATH = orig_libpath)
        conf.env.Replace(CPPPATH = orig_cpppath)
    return False

def FindTR1(conf):
    if conf.CheckCXXHeader('unordered_map'):
        return True
    if conf.CheckCXXHeader('tr1/unordered_map'):
        conf.env.Append(CPPDEFINES=['HAS_TR1'])
        return True
    if conf.CheckCXXHeader('boost/unordered_map.hpp'):
        conf.env.Append(CPPDEFINES=['HAS_BOOST_TR1'])
        return True
    return False

# Find a preferred compiler
def FindCXX(conf, compilers = ['clang++', 'g++']):
    compilers.append(conf.env['CXX'])
    if 'CXX' in os.environ:
        compilers.insert(0, os.environ['CXX'])
    for cxx in compilers:
        conf.env['CXX'] = cxx
        print('Checking if we can compile with %s' % cxx)
        if conf.CheckCXX():
            return True
    return False

# pkg-config is broken for MacFUSE
def FindFUSE(conf):
    if conf.CheckPkg('fuse'):
        return True
    if not IsMac(conf):
        return False

    mac_flags = ' -D_FILE_OFFSET_BITS=64 -D__DARWIN_64_BIT_INO_T=1 '
    if conf.CheckLib('fuse_ino64'):
        mac_flags += ' -D__FreeBSD__=10 '
    elif conf.CheckLib('osxfuse'):
        conf.env.Append(CPPPATH = '/usr/local/include/osxfuse')
    else:
        return False

    conf.env.Append(CPPFLAGS = mac_flags)
    return True
