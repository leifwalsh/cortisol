/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <condition_variable>
#include <queue>
#include <thread>

namespace cortisol {

namespace util {

using std::condition_variable;
using std::lock_guard;
using std::mutex;
using std::queue;
using std::unique_lock;

template<class T>
class Queue {
    queue<T> _q;
    condition_variable _push_cond, _front_cond;
    mutex _mutex;
    const typename queue<T>::size_type _max_size;
  public:
    Queue(typename queue<T>::size_type max_size) : _max_size(max_size) {}
    bool empty() {
        lock_guard<mutex> lk(_mutex);
        return _q.empty();
    }
    typename queue<T>::size_type size() {
        lock_guard<mutex> lk(_mutex);
        return _q.size();
    }
    T &front() {
        unique_lock<mutex> lk(_mutex);
        while (_q.size() == 0) {
            _front_cond.wait(lk);
        }
        return _q.front();
    }
    T &back() {
        lock_guard<mutex> lk(_mutex);
        return _q.back();
    }
    template<class U>
    void push(U &&x) {
        unique_lock<mutex> lk(_mutex);
        while (_q.size() == _max_size) {
            _push_cond.wait(lk);
        }
        _q.push(std::forward<U>(x));
        _front_cond.notify_one();
    }
    void pop() {
        lock_guard<mutex> lk(_mutex);
        _q.pop();
        _push_cond.notify_one();
    }
    void drain() {
        lock_guard<mutex> lk(_mutex);
        while (!_q.empty()) {
            _q.pop();
        }
    }
};

} // namespace util

} // namespace cortisol
