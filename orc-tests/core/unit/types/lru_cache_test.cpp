/*
 * File:        lru_cache_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Unit tests for the thread-safe LRU cache
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>
#include <orc/support/lru_cache.h>

#include <cstdint>
#include <vector>

using orc::LRUCache;

// ---------------------------------------------------------------------------
// put / get basics
// ---------------------------------------------------------------------------

TEST(LRUCache, PutAndGetRoundTrip) {
  LRUCache<int, int> cache(4);
  cache.put(1, 100);
  auto v = cache.get(1);
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, 100);
  EXPECT_FALSE(cache.get(2).has_value());
}

TEST(LRUCache, PutReplacesExistingValue) {
  LRUCache<int, int> cache(4);
  cache.put(1, 100);
  cache.put(1, 200);
  auto v = cache.get(1);
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, 200);
  EXPECT_EQ(cache.size(), 1u);
}

TEST(LRUCache, EvictsLeastRecentlyUsedAtCapacity) {
  LRUCache<int, int> cache(2);
  cache.put(1, 100);
  cache.put(2, 200);
  cache.put(3, 300);  // Evicts key 1
  EXPECT_FALSE(cache.contains(1));
  EXPECT_TRUE(cache.contains(2));
  EXPECT_TRUE(cache.contains(3));
}

// ---------------------------------------------------------------------------
// put_if_absent
// ---------------------------------------------------------------------------

TEST(LRUCache, PutIfAbsentInsertsWhenMissing) {
  LRUCache<int, int> cache(4);
  EXPECT_TRUE(cache.put_if_absent(1, 100));
  auto v = cache.get(1);
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, 100);
}

TEST(LRUCache, PutIfAbsentKeepsExistingValue) {
  LRUCache<int, int> cache(4);
  cache.put(1, 100);
  EXPECT_FALSE(cache.put_if_absent(1, 200));
  auto v = cache.get(1);
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, 100);
  EXPECT_EQ(cache.size(), 1u);
}

TEST(LRUCache, PutIfAbsentPreservesOutstandingGetPtrBuffer) {
  // Regression: a check-then-put race between two threads must not replace a
  // stored buffer, otherwise a pointer obtained via get_ptr() by the first
  // thread dangles. put_if_absent must keep the original allocation intact.
  LRUCache<int, std::vector<int16_t>> cache(4);
  cache.put(1, std::vector<int16_t>(1024, 42));

  const std::vector<int16_t>* first = cache.get_ptr(1);
  ASSERT_NE(first, nullptr);
  const int16_t* first_data = first->data();

  EXPECT_FALSE(cache.put_if_absent(1, std::vector<int16_t>(1024, 99)));

  const std::vector<int16_t>* after = cache.get_ptr(1);
  ASSERT_NE(after, nullptr);
  EXPECT_EQ(after->data(), first_data);
  EXPECT_EQ((*after)[0], 42);
}

TEST(LRUCache, PutIfAbsentRefreshesLruOrder) {
  LRUCache<int, int> cache(2);
  cache.put(1, 100);
  cache.put(2, 200);
  // Touch key 1 via put_if_absent (kept, but moved to most-recently-used).
  EXPECT_FALSE(cache.put_if_absent(1, 999));
  cache.put(3, 300);  // Evicts key 2, not key 1
  EXPECT_TRUE(cache.contains(1));
  EXPECT_FALSE(cache.contains(2));
  EXPECT_TRUE(cache.contains(3));
}

TEST(LRUCache, PutIfAbsentEvictsAtCapacity) {
  LRUCache<int, int> cache(2);
  EXPECT_TRUE(cache.put_if_absent(1, 100));
  EXPECT_TRUE(cache.put_if_absent(2, 200));
  EXPECT_TRUE(cache.put_if_absent(3, 300));  // Evicts key 1
  EXPECT_FALSE(cache.contains(1));
  EXPECT_TRUE(cache.contains(2));
  EXPECT_TRUE(cache.contains(3));
  EXPECT_EQ(cache.size(), 2u);
}
