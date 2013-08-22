cortisol
========

A stress test framework for MongoDB.

This script will stress a MongoDB server with multiple connections doing various operations.
Some examples are point queries (`db.coll.find({'_id': x})`), overwrite inserts (`db.coll.save({'_id': x, ...})`), and table scans (`db.coll.find()`).

It should be easy to extend with new operations, and is meant for both stress testing and performance testing.

Dependencies
------------

You need boost 1.49.  You can specify this with `--boost=/path/to/boost/prefix`.

You also need [TokuKV][tokukv] installed to `./tokukv`.

You also need to unpack mongo-cxx-driver.tgz (from [Tokutek/mongo][tokumx]) to `./mongo-cxx-driver`.

[tokukv]: http://github.com/Tokutek/ft-index
[tokumx]: http://github.com/Tokutek/mongo

Building
--------

To build, just run `scons`.  Add `--d` for debugging symbols.

You can use `--boost` to specify where boost is installed.  If you have a tokumx build directory, you can use the version of boost installed there but it's a little annoying.  For example:
```sh
CC=gcc47 CXX=g++47 scons \
    --boost=/home/leif/git/mongo/build/linux2/release/third_party/boost \
    --extrapath=/home/leif/git/mongo/src/third_party/boost \
    -j5 cortisol
```

Usage
-----

Typical usage would be something like

    $ ./cortisol --documents=10000000 --point_query.threads=8 --seconds=120

This will fill some collections with ten million documents each (with some indexes), and then start 8 threads per collection, doing random point queries, for two minutes.
The performance of the point query threads is printed at the end of the run.

The help text displays all of the options.

Configuration
-------------

You can use a file ./cortisol.cnf to avoid typing lots of things on the command line.
Command line options, when specified, override the configuration parameters.

See [cortisol.cnf](http://github.com/leifwalsh/cortisol/blob/master/cortisol.cnf) for an example.

Output
------

By default, output is TSV.  You can control the output with `--pad-output=[yes|no]`, `--ofs=<sep>`, and `--ors=<sep>`.

To get CSV, try `--pad-output=no --ofs=,`.