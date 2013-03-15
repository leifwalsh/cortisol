#!/usr/bin/env python2.7

import argparse
import collections
import itertools
import logging
import multiprocessing
import pymongo
import random
import time

minval=-1000000
maxval=1000000

# config
c = {
    'collections': 4,
    'indexes': 4,
    'fields': 2,
    'documents': 2**20,
    'seconds': 600,
    'ptquery': {
        'threads': 4,
        'batch': 50
        },
    'scan': {
        'threads': 2,
        },
    'update': {
        'threads': 4,
        'batch': 50
        },
    'drop': {
        'threads': 1,
        'period': 60
        }
    }

fields = 'abcdefghijklmnopqrstuvwxyz'
assert c['fields'] <= len(fields), 'too many fields'

def index_key(i):
    '''Generate a key by combining fields and directions based on the bit pattern of i.'''
    if i % 2 == 0:
        r = [('a', pymongo.ASCENDING)]
    else:
        r = [('a', pymongo.DESCENDING)]
    x = 1
    i /= 2
    while i > 0:
        if i % 2 == 0:
            d = pymongo.ASCENDING
        else:
            d = pymongo.DESCENDING
        assert x < c['fields'], 'too many indexes'
        r.append((fields[x], d))
        x += 1
        i /= 2
    return r

def chunks(iterator, n):
    '''Given an iterable, yields chunks of size n, where a chunk itself is iterable.'''
    iterator = iter(iterator)
    for first in iterator:
        chunk = itertools.chain((first,), itertools.islice(iterator, n-1))
        yield chunk
        collections.deque(chunk, 0)

class CollectionThread(multiprocessing.Process):
    '''A thread that operates on a collection.'''
    def __init__(self, coll):
        multiprocessing.Process.__init__(self)
        self.coll = coll

    @property
    def name(self):
        return '%s(%s)' % (self.__class__.__name__, self.coll.fullname)

    def _run(self):
        raise NotImplementedError

    def run(self):
        logging.debug('Starting %s', self.name)
        try:
            self._run()
        except KeyboardInterrupt:
            return
        except:
            logging.exception('Exception in %s:', self.name)
            raise
        logging.debug('Stopping %s', self.name)

class FillThread(CollectionThread):
    '''Fills a collection.'''
    def _run(self):
        self.coll.fill()

class StressThread(CollectionThread):
    '''A CollectionThread that runs a repeated operation until a timer expires.'''
    def __init__(self, coll, stop_event, queue):
        CollectionThread.__init__(self, coll)
        self.stop_event = stop_event
        self.queue = queue
        self.cnt = collections.Counter()

    def __getattr__(self, attr):
        try:
            return c[self.key][attr]
        except KeyError:
            raise AttributeError

    def sleep(self, secs):
        t0 = time.time()
        while not self.stop_event.is_set() and time.time() - t0 < secs:
            time.sleep(1)

    def performance_inc(self, key, n=1):
        self.cnt[key] += n

    def _run(self):
        try:
            while not self.stop_event.is_set():
                self.step()
        finally:
            self.queue.put(self.cnt)

def sampleids(n):
    return random.sample(xrange(c['documents']), n)

class UpdateThread(StressThread):
    key = 'update'
    def step(self):
        # TODO: Construct some invariant-preserving update statement.
        d = {}
        for f in fields[:c['fields']]:
            d[f] = random.randint(minval, maxval)
        self.coll.update({'_id': {'$in': sampleids(self.batch)}}, {'$inc': d}, multi=True)
        self.performance_inc('updates', self.batch)

class ScanThread(StressThread):
    key = 'scan'
    def step(self):
        # Without the sum, might not force the cursor to iterate over everything.
        suma = sum(item['a'] for item in self.coll.find())
        self.performance_inc('scans')

class PointQueryThread(StressThread):
    key = 'ptquery'
    def step(self):
        # Without the sum, might not force the cursor to iterate over everything.
        suma = sum(item['a'] for item in self.coll.find({'_id': {'$in': sampleids(self.batch)}}))
        self.performance_inc('ptqueries', self.batch)

class DropThread(StressThread):
    key = 'drop'
    def step(self):
        self.sleep(self.period)
        if not self.stop_event.is_set():
            self.coll.drop()
            self.coll.ensure_indexes()

class Collection(pymongo.collection.Collection):
    '''A collection that understands our schema.'''
    def __init__(self, db, i):
        name = 'coll%d' % i
        pymongo.collection.Collection.__init__(self, db, name)
        logging.info('Initializing %s', self.fullname)
        self.threads = []
        self.ensure_indexes()

    @property
    def fullname(self):
        return '%s.%s' % (self.database.name, self.name)

    def ensure_indexes(self):
        for i in range(c['indexes']):
            k = index_key(i)
            logging.debug('creating index %s', k)
            self.create_index(k)

    def fill(self):
        logging.info('Filling %s', self.fullname)
        def zero_doc(i):
            d = {'_id': i}
            for f in fields[:c['fields']]:
                d[f] = 0
            return d

        docs = (zero_doc(i) for i in xrange(c['documents']))

        inserted = 0
        for chunk in chunks(docs, 10000):
            self.insert(chunk, manipulate=False)
            inserted = min(inserted + 10000, c['documents'])
            logging.debug('Filling %s %d/%d', self.fullname, inserted, c['documents'])

def setup(db):
    colls = []
    for i in range(c['collections']):
        colls.append(Collection(db, i))
    return colls

def fill(colls):
    threads = []
    try:
        for coll in colls:
            t = FillThread(coll)
            threads.append(t)
            t.start()
    finally:
        for t in threads:
            t.join()

def stress(colls):
    stop_event = multiprocessing.Event()
    cnt = collections.Counter()
    queue = multiprocessing.Queue()
    threads = []
    t0 = time.time()
    try:
        logging.info('Starting stress test')
        for coll in colls:
            for klass in [UpdateThread, PointQueryThread, ScanThread, DropThread]:
                for _ in range(c[klass.key]['threads']):
                    t = klass(coll, stop_event, queue)
                    threads.append(t)
                    t.start()
        time.sleep(c['seconds'])
        logging.info('Stopping stress test')
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        while len(threads) > 0:
            t = threads.pop()
            t.join()
        while not queue.empty():
            cnt.update(queue.get())

    runtime = time.time() - t0
    for name, ops in sorted(cnt.items()):
        print '%d\t%f %s/s' % (ops, float(ops)/runtime, name)

def main():
    parser = argparse.ArgumentParser(description='Stress test for mongodb.')
    parser.add_argument('--verbose', '-v', action='count')

    conn_args = parser.add_argument_group('Connection')
    conn_args.add_argument('--host', default='127.0.0.1', type=str)
    conn_args.add_argument('--port', default=27017, type=int)

    exec_args = parser.add_argument_group('Execution')
    stress_or_create = exec_args.add_mutually_exclusive_group()
    stress_or_create.add_argument('--only-stress', action='store_true')
    stress_or_create.add_argument('--only-create', action='store_true')
    exec_args.add_argument('--keep-database', action='store_true')

    def add_conf_item(arg_group, d, key, val, prefix=None):
        if isinstance(val, collections.MutableMapping):
            groupname = '%s Thread Configuration' % key.capitalize()
            nested_group = parser.add_argument_group(groupname)
            for nk, nv in val.items():
                add_conf_item(nested_group, val, nk, nv, '%s-' % key)
        else:
            if prefix is None:
                flag = '%s' % key
            else:
                flag = '%s%s' % (prefix, key)
            def setter_call(self, parser, namespace, values, option_string=None):
                self.the_dest[self.the_destkey] = values
            setter_class = type('%s-setter' % flag, (argparse.Action,),
                                {'the_dest': d, 'the_destkey': key, '__call__': setter_call})
            arg_group.add_argument('--%s' % flag, default=val, type=type(val), action=setter_class)

    conf_args = parser.add_argument_group('Configuration')
    for topkey, topval in c.items():
        add_conf_item(conf_args, c, topkey, topval)

    args = parser.parse_args()

    if args.verbose is None:
        args.verbose = 0
    log_levels = [logging.WARNING, logging.INFO, logging.DEBUG]
    logging.basicConfig(level=log_levels[args.verbose])

    try:
        conn = pymongo.MongoClient(args.host, args.port)

        colls = setup(conn.stress_test)
        if not args.only_stress:
            conn.drop_database('stress_test')
            fill(colls)
        if not args.only_create:
            stress(colls)
    finally:
        if not (args.only_create or args.keep_database):
            conn.drop_database('stress_test')

        conn.close()

if __name__ == '__main__':
    main()
