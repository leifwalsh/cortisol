cortisol
========

A stress test framework for MongoDB.

This script will stress a MongoDB server with multiple connections doing various operations.
Some examples are point queries (`db.coll.find({'_id': x})`), overwrite inserts (`db.coll.save({'_id': x, ...})`), and table scans (`db.coll.find()`).

It should be easy to extend with new operations, and is meant for both stress testing and performance testing.

Usage
-----

Typical usage would be something like

    $ ./cortisol.py --documents 10000000 --pointquery-threads 8 --seconds 120

This will fill some collections with ten million documents each (with some indexes), and then start 8 threads per collection, doing random point queries, for two minutes.
The performance of the point query threads is printed at the end of the run.

The help text displays all of the options.

Configuration
-------------

Configuration files are just a set of command line arguments in a file, with optional newlines and comments starting with `#`.
To use a configuration file named `foo.cnf`, add `@foo.cnf` to the command line.
Multiple configuration files can be provided at once, later files and options take precedence over earlier ones.

For example, you can define a setup for testing with a file `db.cnf`:

    --collections 1       # one collection
    --indexes 4
    --fields 2            # need two fields to create 4 additional indexes
    
    --documents 1000000
    --padding 250
    --compressibility 4   # should still be in memory

With this, you can run several tests in a row on the same data:

    $ ./cortisol.py @db.cnf --only-create
    $ echo "--only-stress --keep-database --seconds 120" > stress.cnf
    $ ./cortisol.py @db.cnf @stress.cnf --pointquery-threads 1
    $ ./cortisol.py @db.cnf @stress.cnf --pointquery-threads 2
