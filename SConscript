# -*- mode: python -*-

import sys

Import('env')
Import('debug')
Import('boost')

boostLibs = ["thread", "filesystem", "system"]
conf = Configure(env)
for lib in boostLibs:
    if not conf.CheckLib(["boost_%s-mt" % lib, "boost_%s" % lib],
                         language="C++"):
        Exit(1)
conf.Finish()

mongoEnv = env.Clone()
mongoEnv.Prepend(CPPPATH=['.', 'mongo'])
mongoEnv.Append(CPPDEFINES=['_SCONS', 'MONGO_EXPOSE_MACROS'])
mongoEnv['PYTHON'] = sys.executable
mongoClientEnv = mongoEnv.Clone()
mongoClientEnv['CPPDEFINES'].remove('MONGO_EXPOSE_MACROS')

SConscript('mongo-cxx-driver/src/SConscript.client', exports={'env': mongoEnv, 'clientEnv': mongoClientEnv})

env.Append(CPPDEFINES={'_GNU_SOURCE': 1})
env.Append(CFLAGS=['-std=c99'])
env.Append(CCFLAGS=['-pthread', '-Wall', '-Wno-strict-aliasing'])
env.Append(CXXFLAGS=['-std=c++11', '-Wno-unused-local-typedefs', '-Wno-deprecated-declarations', '-Wno-unused-parameter'])
env.Append(LINKFLAGS=['-static-libgcc', '-static-libstdc++', '-pthread'])

if debug:
    env.Append(CCFLAGS=['-ggdb', '-g3', '-O0'])
else:
    env.Append(CCFLAGS=['-ggdb', '-g', '-O2'])

env.Prepend(CPPPATH=['.', 'mongo-cxx-driver/src'])
env.Append(LIBPATH=['mongo-cxx-driver/src'])

env.Install('#/', env.Program('cortisol',
                              ['collection.cpp',
                               'cortisol.cpp',
                               'main.cpp',
                               'options.cpp',
                               'output.cpp',
                               'timing.c',
                               'words.cpp'],
                              LIBDEPS=['mongo-cxx-driver/src/mongoclient'],
                              LIBS=['jemalloc_pic', 'mongoclient', 'boost_thread', 'boost_filesystem', 'boost_system', 'boost_program_options']))
