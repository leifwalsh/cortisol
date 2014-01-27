cmake_minimum_required(VERSION 2.8)

project(cortisol)

set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")

include(CheckCXXCompilerFlag)
macro(set_cxxflags_if_supported)
  foreach(flag ${ARGN})
    check_cxx_compiler_flag(${flag} HAVE_CXX_${flag})
    if (HAVE_CXX_${flag})
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
    endif ()
  endforeach(flag)
endmacro(set_cxxflags_if_supported)

set_cxxflags_if_supported(
  -Wall
  -Werror
  -Wno-deprecated-declarations
  -Wno-unused-local-typedefs
  )

#set(BOOST_INCLUDEDIR /home/leif/git/mongo/src/third_party/boost)
#set(BOOST_LIBRARYDIR /home/leif/git/mongo/opt/src/third_party/boost)
#set(Boost_NO_SYSTEM_PATHS ON)
find_package(Boost 1.48 REQUIRED
  COMPONENTS system thread filesystem program_options
  )
find_package(Threads REQUIRED)

find_path(MONGOCLIENT_INCLUDE_DIR mongo/client/dbclient.h ${TOKUMX_PREFIX}/include)
find_library(MONGOCLIENT_LIBRARY mongoclient ${TOKUMX_PREFIX}/lib64)

include_directories(
  ${Boost_INCLUDE_DIRS}
  ${MONGOCLIENT_INCLUDE_DIR}
  .
  )

add_executable(cortisol
  cortisol
  main
  runner
  setup
  thread_interrupter
  util/output
  util/timing

  # plugins
  tokumx
  )
target_link_libraries(cortisol
  ${MONGOCLIENT_LIBRARY}
  ${Boost_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  )
