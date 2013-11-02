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

#include <cxxll/bounded_ordered_queue.hpp>
#include <cxxll/task.hpp>
#include "test.hpp"

#include <vector>
#include <set>

#include <unistd.h>

using namespace cxxll;

namespace {

  const int max_id = 15;
  const int count = 250;

  struct background {
    bounded_ordered_queue<int, int> &boq;
    int id;
    background(bounded_ordered_queue<int, int> &q, int i)
      : boq(q), id(i)
    {
    }

    void operator()() throw()
    {
      for (int i = 0; i < count; ++i) {
	boq.push(i * max_id + id, id);
	if (i % (count / 5) == 0) {
	  usleep(10 * 1000);
	}
      }
      boq.remove_producer();
    }
  };
}

static void
test()
{
  {
    bounded_ordered_queue<int, int> boq(5);
    CHECK(boq.size_estimate() == 0);
    CHECK(boq.producers() == 1);
    boq.add_producer();
    CHECK(boq.producers() == 2);
    boq.remove_producer();
    CHECK(boq.producers() == 1);
    boq.remove_producer();
    CHECK(boq.producers() == 0);
    try {
      boq.remove_producer();
      CHECK(false);
    } catch (queue_without_producers &) {
      CHECK(true);
    }
    try {
      boq.push(3, 4);
      CHECK(false);
    } catch (std::exception &) {
      CHECK(true);
    }
    CHECK(boq.size_estimate() == 0);
    int k = 0, v = 0;
    CHECK(!boq.pop(k, v));
    CHECK(k == 0);
    CHECK(v == 0);
    boq.add_producer();
    boq.push(1, 2);
    CHECK(boq.pop(k, v));
    CHECK(k == 1);
    CHECK(v == 2);
    boq.push(7, 8);
    boq.remove_producer();
    CHECK(boq.pop(k, v));
    CHECK(k == 7);
    CHECK(v == 8);
    CHECK(!boq.pop(k, v));
    CHECK(k == 7);
    CHECK(v == 8);
    CHECK(boq.size_estimate() == 0);
    boq.add_producer();
    boq.push(11, 1);
    boq.push(10, 2);
    boq.push(14, 3);
    boq.push(12, 4);
    boq.push(13, 5);
    CHECK(boq.size_estimate() == 5);
    CHECK(boq.pop() == std::make_pair(10, 2));
    CHECK(boq.pop() == std::make_pair(11, 1));
    CHECK(boq.pop() == std::make_pair(12, 4));
    CHECK(boq.pop() == std::make_pair(13, 5));
    CHECK(boq.pop() == std::make_pair(14, 3));
    CHECK(boq.size_estimate() == 0);
    boq.remove_producer();
    try {
      boq.pop();
      CHECK(false);
    } catch (queue_without_producers &) {
      CHECK(true);
    }
  }

  bounded_ordered_queue<int, int> boq(max_id, max_id);
  std::vector<std::unique_ptr<task> > tasks;
  for (int k = 0; k < max_id; ++k) {
    tasks.push_back(std::unique_ptr<task>(new task(background(boq, k))));
  }
  std::set<int> results;
  {
    int k, v;
    while (boq.pop(k, v)) {
      CHECK((k % max_id) == v);
      CHECK(boq.size_estimate() <= static_cast<unsigned>(max_id));
      results.insert(k);
      if (results.size() % (max_id * count / 3) == 0) {
	usleep(10 * 1000);
      }
    }
  }
  CHECK(results.size() == max_id * count);
  CHECK(*results.begin() == 0);
  CHECK(*(--results.end()) == max_id * count - 1);
  for (int k = 0; k < max_id; ++k) {
    tasks.at(k)->wait();
  }
}

static test_register t("bounded_ordered_queue", test);
