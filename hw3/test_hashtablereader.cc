/*
 * Copyright ©2025 Hal Perkins.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Spring Quarter 2025 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <unistd.h>

#include <cstdio>
#include <iostream>
#include <list>
#include <string>

#include "gtest/gtest.h"
#include "./HashTableReader.h"
#include "./LayoutStructs.h"
#include "./test_suite.h"
#include "./Utils.h"

using std::list;
using std::string;

namespace hw3 {

class Test_HashTableReader : public ::testing::Test {
 public:
  list<IndexFileOffset_t> LookupElementPositions(HashTableReader* htr,
                                                 HTKey_t hashval) {
    return htr->LookupElementPositions(hashval);
  }
};


TEST_F(Test_HashTableReader, TestHashTableReaderBasic) {
  HW3Environment::OpenTestCase();

  // Open up the FILE* for enron.idx
  string idx("unit_test_indices/enron.idx");
  FILE* f = fopen(idx.c_str(), "rb");
  ASSERT_NE(nullptr, f);

  // Prep the HashTableReader to point to the docid-->docname table,
  // which is at offset sizeof(IndexFileHeader) in the file.
  HashTableReader htr(f, sizeof(IndexFileHeader));

  // Do a couple of bucket lookups.
  list<IndexFileOffset_t> res = LookupElementPositions(&htr, 5);
  ASSERT_EQ(1U, res.size());
  ASSERT_EQ(11845, *(res.begin()));

  unsigned char* word = (unsigned char*) "rain";
  HTKey_t word_hash = FNVHash64(word, 4);
  res = LookupElementPositions(&htr, word_hash);
  ASSERT_EQ(1U, res.size());
  // check that the position is what is expected.
  ASSERT_EQ(46396, *(res.begin()));

  res = LookupElementPositions(&htr, 6);
  ASSERT_EQ(1U, res.size());
  ASSERT_EQ(11885, *(res.begin()));

  // Done!
  HW3Environment::AddPoints(20);
}

}  // namespace hw3
