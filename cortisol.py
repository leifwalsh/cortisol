#!/usr/bin/env python2.7

import argparse
import bson
import bson.binary
import collections
import itertools
import logging
import multiprocessing
import os
import pymongo
import Queue
import random
import re
import shlex
import sys
import threading
import time

MINVAL = -1000000
MAXVAL = 1000000

class Config(argparse.Namespace):
    pass

conf = Config(
    collections = 4,
    indexes = 4,
    fields = 2,
    documents = 2**20,
    seconds = 600,
    padding = 200,
    compressibility = 0.25
    )

fields = 'abcdefghijklmnopqrstuvwxyz'
assert conf.fields <= len(fields), 'too many fields'

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
        assert x < conf.fields, 'too many indexes'
        r.append((fields[x], d))
        x += 1
        i /= 2
    return r

class ChunkGenerator(threading.Thread):
    def __init__(self, iterable, chunksize, queue):
        threading.Thread.__init__(self)
        self.iterable = iterable
        self.chunksize = chunksize
        self.queue = queue

    @staticmethod
    def chunks(iterator, n):
        '''Given an iterable, yields chunks of size n, where a chunk itself is iterable.'''
        iterator = iter(iterator)
        for first in iterator:
            chunk = itertools.chain((first,), itertools.islice(iterator, n-1))
            yield chunk
            collections.deque(chunk, 0)

    def run(self):
        try:
            for chunk in self.chunks(self.iterable, self.chunksize):
                # Force the computation on this thread
                self.queue.put(list(chunk))
        except KeyboardInterrupt:
            return

def chunks(iterable, chunksize):
    '''Given a (lazy) iterable, and a chunk size, generates and yields chunks of that size and forces the computation on a background thread.'''

    q = Queue.Queue(maxsize=3)
    t = ChunkGenerator(iterable, chunksize, q)
    t.start()
    while t.is_alive() or not q.empty():
        try:
            chunk = q.get(True, 1)
            yield chunk
        except Queue.Empty:
            pass
    t.join()

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

    def sleep(self, secs):
        t0 = time.time()
        while not self.stop_event.is_set() and time.time() - t0 < secs:
            time.sleep(1)

    def performance_inc(self, key, n=1):
        self.cnt[key] += n

    def generate(self):
        raise NotImplementedError

    def _run(self):
        try:
            self.chunks = chunks(self.generate(), self.batch)
        except NotImplementedError:
            pass
        try:
            while not self.stop_event.is_set():
                self.step()
        finally:
            self.queue.put(self.cnt)

def sampleids(n):
    return random.sample(xrange(conf.documents), n)

class UpdateThread(StressThread):
    config = ['threads', 'batch']
    threads = 0
    batch = 50
    def generate(self):
        while True:
            d = {}
            for f in fields[:conf.fields]:
                d[f] = random.randint(MINVAL, MAXVAL)
            iddoc = {'_id': 0}
            incdoc = {'$inc': d}
            for i in sampleids(self.batch):
                iddoc['_id'] = i
                yield iddoc, incdoc

    def step(self):
        # TODO: Construct some invariant-preserving update statement.
        for iddoc, incdoc in next(self.chunks):
            self.coll.update(iddoc, incdoc)
        self.performance_inc('updates', self.batch)

class SaveThread(StressThread):
    config = ['threads', 'batch']
    threads = 0
    batch = 50
    def generate(self):
        while True:
            d = {'_id': random.randint(0, conf.documents)}
            for f in fields[:conf.fields]:
                d[f] = random.randint(MINVAL, MAXVAL)
            compressible_bytes = int(conf.padding * conf.compressibility)
            s = '0' * compressible_bytes
            s += os.urandom(conf.padding - compressible_bytes)
            d['pad'] = bson.binary.Binary(s)
            yield d

    def step(self):
        for doc in next(self.chunks):
            self.coll.save(doc)
        self.performance_inc('saves', self.batch)

class ScanThread(StressThread):
    config = ['threads']
    threads = 0
    def step(self):
        # Without the sum, might not force the cursor to iterate over everything.
        suma = sum(item['a'] for item in self.coll.find())
        self.performance_inc('scans')

class PointQueryThread(StressThread):
    config = ['threads', 'batch']
    threads = 0
    batch = 50
    def generate(self):
        while True:
            yield {'_id': random.randint(0, conf.documents)}

    def step(self):
        # Without the sum, might not force the cursor to iterate over everything.
        suma = 0
        for iddoc in next(self.chunks):
            for doc in self.coll.find(iddoc):
                suma += doc['a']
        self.performance_inc('ptqueries', self.batch)

class DropThread(StressThread):
    config = ['threads', 'period']
    threads = 0
    period = 60
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
        for i in range(conf.indexes):
            k = index_key(i)
            logging.debug('creating index %s', k)
            self.create_index(k)

    def fill(self):
        logging.info('Filling %s', self.fullname)
        def zero_doc(i):
            d = {'_id': i}
            for f in fields[:conf.fields]:
                d[f] = 0
            compressible_bytes = int(conf.padding * conf.compressibility)
            s = '0' * compressible_bytes
            s += os.urandom(conf.padding - compressible_bytes)
            d['pad'] = bson.binary.Binary(s)
            return d

        docs = (zero_doc(i) for i in xrange(conf.documents))

        inserted = 0
        for chunk in chunks(docs, 100000):
            self.insert(chunk, manipulate=False)
            inserted = min(inserted + 100000, conf.documents)
            logging.debug('Filling %s %d/%d', self.fullname, inserted, conf.documents)

def setup(db):
    colls = []
    for i in range(conf.collections):
        colls.append(Collection(db, i))
    return colls

def fill(colls):
    threads = []
    t0 = time.time()
    try:
        for coll in colls:
            t = FillThread(coll)
            threads.append(t)
            t.start()
    finally:
        for t in threads:
            t.join()

    runtime = time.time() - t0
    ops = conf.documents * conf.collections
    print '%d\t%f inserts/s' % (ops, float(ops)/runtime)

def stress(colls):
    stop_event = multiprocessing.Event()
    cnt = collections.Counter()
    queue = multiprocessing.Queue()
    threads = []
    t0 = time.time()
    try:
        logging.info('Starting stress test')
        for coll in colls:
            for cls in StressThread.__subclasses__():
                for _ in range(cls.threads):
                    t = cls(coll, stop_event, queue)
                    threads.append(t)
                    t.start()
        time.sleep(conf.seconds)
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

    def add_conf_item(arg_group, obj, key, default_value, prefix=None):
        if prefix is None:
            flag = '%s' % key
        else:
            flag = '%s-%s' % (prefix, key)
        def setter_call(self, parser, namespace, values, option_string=None):
            setattr(self.obj, self.key, values)
        setter_class = type('%s-setter' % flag, (argparse.Action,),
                            {'obj': obj, 'key': key, '__call__': setter_call})
        arg_group.add_argument('--%s' % flag, default=default_value,
                               type=type(default_value), action=setter_class)

    conf_args = parser.add_argument_group('Configuration')
    for k, v in conf.__dict__.items():
        add_conf_item(conf_args, conf, k, v)

    for cls in StressThread.__subclasses__():
        match = re.match('([a-zA-Z0-9]+)Thread', cls.__name__)
        capitalized = match.group(1)
        lowercase = capitalized.lower()
        cls_args = parser.add_argument_group('%s Thread Configuration' % capitalized)
        for k in cls.config:
            v = getattr(cls, k)
            add_conf_item(cls_args, cls, k, v, lowercase)

    def expandargs(args):
        def expandarg(arg):
            if arg[0] == '@':
                with open(arg[1:], 'r') as f:
                    data = f.read()
                return expandargs(shlex.split(data, '#'))
            else:
                return [arg]
        return itertools.chain.from_iterable(expandarg(arg) for arg in args)

    args = parser.parse_args(list(expandargs(sys.argv[1:])))

    if args.verbose is None:
        args.verbose = 0
    log_levels = [logging.WARNING, logging.INFO, logging.DEBUG]
    logging.basicConfig(level=log_levels[args.verbose])

    conn = pymongo.MongoClient(args.host, args.port)

    try:
        if not args.only_stress:
            conn.drop_database('stress_test')
        colls = setup(conn.stress_test)
        if not args.only_stress:
            fill(colls)
        if not args.only_create:
            stress(colls)
    finally:
        if not (args.only_create or args.keep_database):
            conn.drop_database('stress_test')

        conn.close()

if __name__ == '__main__':
    main()
