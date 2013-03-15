#!/usr/bin/env python2.7

import argparse
import collections
import itertools
import logging
import multiprocessing
import pymongo
import random
import sys
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
    def __init__(self, coll):
        multiprocessing.Process.__init__(self)
        self.coll = coll

    @property
    def name(self):
        return '%s(%s)' % (self.__class__.__name__, self.coll.fullname)

class FillThread(CollectionThread):
    def run(self):
        try:
            self.coll.fill()
        except KeyboardInterrupt, e:
            pass
        except:
            logging.exception('Exception in %s:', self.name)
            raise

class Runner(CollectionThread):
    def __init__(self, coll, stop_event):
        CollectionThread.__init__(self, coll)
        self.stop_event = stop_event

    def run(self):
        logging.debug('Starting %s', self.name)
        while not self.stop_event.is_set():
            try:
                self.step()
            except KeyboardInterrupt, e:
                break
            except:
                logging.exception('Exception in %s:', self.name)
                raise
        logging.debug('Stopping %s', self.name)

def sampleids(n):
    return random.sample(xrange(c['documents']), n)

class UpdateThread(Runner):
    tc = c['update']
    def step(self):
        # TODO: Construct some invariant-preserving update statement.
        d = {}
        for f in fields[:c['fields']]:
            d[f] = random.randint(minval, maxval)
        self.coll.update({'_id': {'$in': sampleids(self.tc['batch'])}}, {'$inc': d}, multi=True)

class ScanThread(Runner):
    tc = c['scan']
    def step(self):
        # Without the sum, might not force the cursor to iterate over everything.
        suma = sum(item['a'] for item in self.coll.find())

class PointQueryThread(Runner):
    tc = c['ptquery']
    def step(self):
        suma = sum(item['a'] for item in self.coll.find({'_id': {'$in': sampleids(self.tc['batch'])}}))

class Timeout(Exception):
    pass

class DropThread(Runner):
    tc = c['drop']
    def step(self):
        try:
            self.waitfor(self.tc['period'])
            self.coll.drop()
            self.coll.ensure_indexes()
        except Timeout, e:
            return

    def waitfor(self, secs):
        t0 = time.time()
        while not self.stop_event.is_set() and time.time() - t0 < secs:
            time.sleep(1)
        if self.stop_event.is_set():
            raise Timeout()

class Collection(pymongo.collection.Collection):
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
            logging.debug('creating index %s' % k)
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
            logging.debug('Filling %s %d/%d' % (self.fullname, inserted, c['documents']))

    def start_threads(self, stop_event):
        for klass in [UpdateThread, PointQueryThread, ScanThread, DropThread]:
            for i in range(klass.tc['threads']):
                t = klass(self, stop_event)
                self.threads.append(t)
                t.start()

    def join_threads(self):
        while len(self.threads) > 0:
            t = self.threads.pop()
            t.join()

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
    try:
        logging.info('Starting stress test')
        for coll in colls:
            coll.start_threads(stop_event)
        time.sleep(10)
        logging.info('Stopping stress test')
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        for coll in colls:
            coll.join_threads()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Stress test for mongodb.')
    parser.add_argument('--host', default='127.0.0.1', type=str)
    parser.add_argument('--port', default=27017, type=int)
    parser.add_argument('--verbose', '-v', action='count')
    # TODO: Maybe construct arguments out of config hash.
    args = parser.parse_args()

    if args.verbose is None:
        args.verbose = 0
    log_levels = [logging.WARNING, logging.INFO, logging.DEBUG]
    logging.basicConfig(level=log_levels[args.verbose])

    conn = pymongo.MongoClient(args.host, args.port)
    conn.drop_database('stress_test')
    colls = setup(conn.stress_test)
    fill(colls)
    stress(colls)
    conn.drop_database('stress_test')
    conn.close()
