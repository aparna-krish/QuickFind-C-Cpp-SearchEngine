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

#include "./QueryProcessor.h"

#include <iostream>
#include <algorithm>
#include <list>
#include <string>
#include <vector>

extern "C" {
  #include "./libhw1/CSE333.h"
}

using std::list;
using std::sort;
using std::string;
using std::vector;

namespace hw3 {

// Intersect 2 DocIDElementHeader lists and sum their num_positions
static std::list<DocIDElementHeader>
    IntersectHeaders(const std::list<DocIDElementHeader>& l1,
    const std::list<DocIDElementHeader>& l2);

// Given a DocTableReader and list of headers, append QueryResults to out
static void GatherResults(std::vector<QueryProcessor::QueryResult>& out,
                  DocTableReader* dtr,
                  const std::list<DocIDElementHeader>& headers);

QueryProcessor::QueryProcessor(const list<string> &index_list, bool validate) {
  // Stash away a copy of the index list.
  index_list_ = index_list;
  array_len_ = index_list_.size();
  Verify333(array_len_ > 0);

  // Create the arrays of DocTableReader*'s. and IndexTableReader*'s.
  dtr_array_ = new DocTableReader*[array_len_];
  itr_array_ = new IndexTableReader*[array_len_];

  // Populate the arrays with heap-allocated DocTableReader and
  // IndexTableReader object instances.
  list<string>::const_iterator idx_iterator = index_list_.begin();
  for (int i = 0; i < array_len_; i++) {
    FileIndexReader fir(*idx_iterator, validate);
    dtr_array_[i] = fir.NewDocTableReader();
    itr_array_[i] = fir.NewIndexTableReader();
    idx_iterator++;
  }
}

QueryProcessor::~QueryProcessor() {
  // Delete the heap-allocated DocTableReader and IndexTableReader
  // object instances.
  Verify333(dtr_array_ != nullptr);
  Verify333(itr_array_ != nullptr);
  for (int i = 0; i < array_len_; i++) {
    delete dtr_array_[i];
    delete itr_array_[i];
  }

  // Delete the arrays of DocTableReader*'s and IndexTableReader*'s.
  delete[] dtr_array_;
  delete[] itr_array_;
  dtr_array_ = nullptr;
  itr_array_ = nullptr;
}

// This structure is used to store a index-file-specific query result.
typedef struct {
  DocID_t doc_id;  // The document ID within the index file.
  int rank;        // The rank of the result so far.
} IdxQueryResult;

vector<QueryProcessor::QueryResult>
QueryProcessor::ProcessQuery(const vector<string> &query) const {
  Verify333(query.size() > 0);

  // STEP 1.
  // (the only step in this file)
  vector<QueryProcessor::QueryResult> final_result;

  for (int idx = 0; idx < array_len_; ++idx) {
    DocTableReader* dtr = dtr_array_[idx];
    IndexTableReader* itr = itr_array_[idx];

    // Get postings for first word
    DocIDTableReader* dr = itr -> LookupWord(query[0]);
    if (dr == nullptr) {
      continue;
    }

    std::list<DocIDElementHeader> candidates = dr->GetDocIDList();
    delete dr;

    // For each word after intersect candidate sets
    for (size_t w = 1; w < query.size() && !candidates.empty(); ++w) {
      dr = itr->LookupWord(query[w]);
      if (dr == nullptr) {
        candidates.clear();
        break;
      }

      std::list<DocIDElementHeader> nextSet = dr->GetDocIDList();
      delete dr;
      candidates = IntersectHeaders(candidates, nextSet);
    }

    // Look up file names and push into final_results if any words
    if (!candidates.empty()) {
      GatherResults(final_result, dtr, candidates);
    }
  }

  // Sort the final results.
  sort(final_result.begin(), final_result.end());
  return final_result;
}

static void GatherResults(std::vector<QueryProcessor::QueryResult>& out,
               DocTableReader* dtr,
               const std::list<DocIDElementHeader>& headers) {
  // For each header in list
  for (auto it = headers.begin(); it != headers.end(); ++it) { 
    QueryProcessor::QueryResult qr;
    // Use stored num_positions for sorting rank
    qr.rank = it -> num_positions;
    // Look up filename and store in qr.document_name
    dtr -> LookupDocID(it -> doc_id, &qr.document_name);
    // Append QueryResult to output list
    out.push_back(qr);
  }
}

static std::list<DocIDElementHeader> IntersectHeaders(const std::list<DocIDElementHeader>& l1,
                  const std::list<DocIDElementHeader>& l2) {
  std::list<DocIDElementHeader> out;
  // For each header in first list
  for (auto it1 = l1.begin(); it1 != l1.end(); ++it1) {
    // Scan second list for same doc_id
    for (auto it2 = l2.begin(); it2 != l2.end(); ++it2) {
       if (it1->doc_id == it2->doc_id) {
        // Create combined header and summed rank for same document
        out.emplace_back(it1->doc_id,
                          it1->num_positions + it2->num_positions);
         // Break to avoid duplicates if l2 has more same IDs
         break;
       }
     }
  }
  return out;
}

}  // namespace hw3
