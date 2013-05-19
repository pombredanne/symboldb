/*
 * Copyright (C) 2013 Red Hat, Inc.
 * Written by Florian Weimer <fweimer@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cxxll/mutex.hpp>
#include <cxxll/cond.hpp>

#include <map>
#include <stdexcept>

namespace cxxll {

// Thrown by bounded_ordered_queue::remove_producer(), pop() to
// indicate that no producers are left.
class queue_without_producers : public std::exception {
};

template <class Key, class Value>
class bounded_ordered_queue {
  mutable mutex mutex_;
  cond cond_;
  typedef std::multimap<Key, Value> map;
  map map_;
  unsigned capacity_;
  unsigned producers_;
  bounded_ordered_queue(const bounded_ordered_queue &); // not implemented
  bounded_ordered_queue &operator=(const bounded_ordered_queue &); // same
public:
  // Creates a new queue with one producer.
  explicit bounded_ordered_queue(unsigned capacity);
  ~bounded_ordered_queue();

  void add_producer();
  void remove_producer();

  // Returns the number of registered producers.  Initially 1.
  unsigned producers() const;

  // Adds an element to the queue.
  void push(const Key &, const Value &);

  // Tries to remove an element from the queue.  Blocks until more
  // elements are available.  If there are no producers left, throws
  // no_producers if no elements are available.
  std::pair<Key, Value> pop() __attribute__((warn_unused_result));

  // Tries to remove an element from the queue, blocking until one
  // becomes available, and returns true.  Returns false if there are
  // no elements and no producers left.
  bool pop(Key &, Value &) __attribute__((warn_unused_result));

  // Returns an estimate for the number of elements in the queue.
  unsigned size_estimate() const;
};

template <class Key, class Value>
bounded_ordered_queue<Key, Value>::bounded_ordered_queue(unsigned capacity)
  : capacity_(capacity), producers_(1)
{
  if (capacity == 0) {
    throw std::logic_error("bounded_ordered_queue capacity must not be zero");
  }
}

template <class Key, class Value>
bounded_ordered_queue<Key, Value>::~bounded_ordered_queue()
{
}

template <class Key, class Value> void
bounded_ordered_queue<Key, Value>::add_producer()
{
  mutex::locker ml(&mutex_);
  ++producers_;
  if (producers_ == 0) {
    throw std::runtime_error("too many producers");
  }
}

template <class Key, class Value> void
bounded_ordered_queue<Key, Value>::remove_producer()
{
  mutex::locker ml(&mutex_);
  if (producers_ == 0) {
    throw queue_without_producers();
  }
  --producers_;
  if (producers_ == 0) {
    cond_.signal();
  }
}

template <class Key, class Value> unsigned
bounded_ordered_queue<Key, Value>::producers() const
{
  mutex::locker ml(&mutex_);
  return producers_;
}

template <class Key, class Value> void
bounded_ordered_queue<Key, Value>::push(const Key &key, const Value &value)
{
  mutex::locker ml(&mutex_);
  if (producers_ == 0) {
    throw std::logic_error("bounded_ordered_queue push without producers");
  }
  while (map_.size() >= capacity_) {
    cond_.wait(mutex_);
  }
  map_.insert(std::make_pair(key, value));
  cond_.signal();
}

template <class Key, class Value> bool
bounded_ordered_queue<Key, Value>::pop(Key &key, Value &value)
{
  mutex::locker ml(&mutex_);
  while (map_.empty()) {
    while (producers_ > 0) {
      cond_.wait(mutex_);
    }
    if (map_.empty() && producers_ == 0) {
      return false;
    }
  }
  typename map::iterator p(map_.begin());
  key = p->first;
  value = p->second;
  map_.erase(p);
  cond_.signal();
  return true;
}

template <class Key, class Value> std::pair<Key, Value>
bounded_ordered_queue<Key, Value>::pop()
{
  std::pair<Key, Value> result;
  if (!pop(result.first, result.second)) {
    throw queue_without_producers();
  }
  return result;
}


template <class Key, class Value> unsigned
bounded_ordered_queue<Key, Value>::size_estimate() const
{
  mutex::locker ml(&mutex_);
  return map_.size();
}

} // namespace cxxll
