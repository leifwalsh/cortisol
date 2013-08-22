# -*- mode: python -*-

import os
import os.path

AddOption("--d", dest="debug", action="store_true", help="debug build")
AddOption("--boost", dest="boost", type="string", nargs=1, action="store", help="where to find boost libs")
AddOption("--extrapath",
          dest="extrapath",
          type="string",
          nargs=1,
          action="store",
          help="comma separated list of add'l paths  (--extrapath /opt/foo/,/foo) static linking")
AddOption("--prefix",
          dest="prefix",
          type="string",
          nargs=1,
          action="store",
          default="/usr/local",
          help="installation root")

env = Environment(ENV=os.environ, #CPPDEFINES=['MONGO_BOOST_TIME_UTC_HACK'],
                  CC=os.getenv('CC', 'gcc'),
                  CXX=os.getenv('CXX', 'g++'),
                  CPPPATH=['#tokukv/include'],
                  LIBPATH=['#tokukv/lib'])

def add_libs(path):
    env.Prepend(CPPPATH=[os.path.join(path, 'include')])
    env.Prepend(LIBPATH=[os.path.join(path, 'lib'), path])
    env.Prepend(RPATH=[os.path.join(path, 'lib'), path])

boost = GetOption('boost')
if boost is not None:
    add_libs(boost)

def addExtraLibs(s):
    for x in s.split(","):
        if os.path.exists(x):
            env.Append(CPPPATH=[x + "/include", x],
                       LIBPATH=[x + "/lib", x + "/lib64"])

if GetOption( "extrapath" ) is not None:
    addExtraLibs( GetOption( "extrapath" ) )

Export('env')
debug = GetOption('debug')
Export('boost debug')
build_dir = '#build/'
if debug:
    build_dir += 'debug'
else:
    build_dir += 'release'
SConscript('SConscript', variant_dir=build_dir)
