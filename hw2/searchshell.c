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

// Feature test macro for strtok_r (c.f., Linux Programming Interface p. 63)
#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include "libhw1/CSE333.h"
#include "./CrawlFileTree.h"
#include "./DocTable.h"
#include "./MemIndex.h"

//////////////////////////////////////////////////////////////////////////////
// Helper function declarations, constants, etc
static void Usage(void);
// Prints search result with correct ranking and format
static void ProcessQueries(FILE *f, DocTable *dt,
                            MemIndex *mi, char **query, int q_size);
// Reads next line of user input input into return parameter
static int GetNextLine(FILE *f, char **ret_str);

#define QUERY_SIZE 4
#define BUF_SIZE 128


//////////////////////////////////////////////////////////////////////////////
// Main
int main(int argc, char **argv) {
  if (argc != 2) {
    Usage();
  }

  DocTable *doc_tab;
  MemIndex *mem_idx;

  printf("Indexing '%s'\n", argv[1]);
  if (!CrawlFileTree(argv[1], &doc_tab, &mem_idx)) {
    fprintf(stderr, "Failed to crawl given directory");
    return EXIT_FAILURE;
  }

  // Implement searchshell!  We're giving you very few hints
  // on how to do it, so you'll need to figure out an appropriate
  // decomposition into functions as well as implementing the
  // functions.  There are several major tasks you need to build:
  //
  //  - Crawl from a directory provided by argv[1] to produce and index
  //  - Prompt the user for a query and read the query from stdin, in a loop
  //  - Split a query into words (check out strtok_r)
  //  - Process a query against the index and print out the results
  //
  // When searchshell detects end-of-file on stdin (cntrl-D from the
  // keyboard), searchshell should free all dynamically allocated
  // memory and any other allocated resources and then exit.
  //
  // Note that you should make sure the fomatting of your
  // searchshell output exactly matches our solution binaries
  // to get full points on this part.

  while (true) {
    char *words = NULL;

    // Get user input
    printf("enter query:\n");
    if (!GetNextLine(stdin, &words)) {
      if (words != NULL) {
        free(words);
      }

      if (errno == 0) {
        break;
      }

      perror("Failed to read query\n");
      DocTable_Free(doc_tab);
      MemIndex_Free(mem_idx);
      return EXIT_FAILURE;
    }

    // Parse query
    char **query = NULL;
    int q_size;
    int query_capacity = QUERY_SIZE;

    // Allocate memory for query words
    query = (char**) malloc(query_capacity * sizeof(char*));
    if (query == NULL) {
        perror("Memory allocation failed");
        q_size = -1;
    }

    char *save_ptr;
    int count = 0;
    // Tokenize input by space
    char *token = strtok_r(words, " ", &save_ptr);

    while (token != NULL) {
      // Expand queries arrary if needed
      if (count >= query_capacity) {
        query_capacity *= 2;
        char **temp = (char**) realloc(query, query_capacity * sizeof(char*));
        if (temp == NULL) {
          perror("Memory reallocation failed");
          // Free previously allocated strings before returning
          for (int i = 0; i < count; i++) {
            free((query)[i]);
          }
          free(query);
          q_size = -1;
        }
        query = temp;
      }
      (query)[count++] = strdup(token);
      token = strtok_r(NULL, " ", &save_ptr);
    }

    q_size = count;

    if (q_size == -1) {
      free(words);
      printf("Failed to get queries\n");
      return EXIT_FAILURE;
    }

    ProcessQueries(stdout, doc_tab, mem_idx, query, q_size);


    // free query list and the words inside of it
    for (int i = 0; i < q_size; i++) {
      free(query[i]);
    }
    free(query);
    free(words);
  }

  // Free DocTable and MemIndex
  printf("shutting down...\n");
  DocTable_Free(doc_tab);
  MemIndex_Free(mem_idx);
  return EXIT_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////
// Helper function definitions

static void Usage(void) {
  fprintf(stderr, "Usage: ./searchshell <docroot>\n");
  fprintf(stderr,
          "where <docroot> is an absolute or relative " \
          "path to a directory to build an index under.\n");
  exit(EXIT_FAILURE);
}

static void ProcessQueries(FILE *f, DocTable *dt,
                           MemIndex *mi, char **query, int q_size) {
  LinkedList *ret = MemIndex_Search(mi, query, q_size);

  if (ret != NULL) {
    // Iterate through the list from the MemIndex_Search
    LLIterator *iter = LLIterator_Allocate(ret);
    while (LLIterator_IsValid(iter)) {
      // Print file path and rank
      SearchResult *search_res;
      LLIterator_Get(iter, (LLPayload_t *) &search_res);
      // Get file path to doc_id with HashTable
      char *path = DocTable_GetDocName(dt, search_res -> doc_id);
      fprintf(f, " %s (%d)\n", path, search_res -> rank);
      LLIterator_Next(iter);
    }

    // Free when done
    LLIterator_Free(iter);
    LinkedList_Free(ret, free);
  }
}

static int GetNextLine(FILE *f, char **ret_str) {
  size_t buf_size = BUF_SIZE;
  size_t len = 0;

  // Allocate memory to hold input line
  char *buffer = (char*) malloc(buf_size * sizeof(char));

  if (buffer == NULL) {
      perror("Memory allocation failed");
      return 0;
  }

  int c;

  // Read each char from input file
  while ((c = fgetc(f) != EOF)) {
    // Doubel buffer size if curr length reaches buffer size limit
    if (len >= buf_size - 1) {
      buf_size *= 2;
      char *temp = realloc(buffer, buf_size);
      if (temp == NULL) {
        free(buffer);
        perror("memory allocation failed");
        return 0;
      }

      buffer = temp;
    }

    // Convert char to lowercse and add to buffer
    buffer[len++] = (char) tolower(c);

    // If new line, stop reading
    if (c == "\n") {
      break;
    }
  }

  if (len == 0 && c == EOF) {
    free(buffer);
    return 0;
  }

  // Add null terminator to end of string
  buffer[len] = "\0";

  // Remove new line character
  if (len > 0 && buffer[len - 1] == "\n") {
    buffer[len - 1] = "\0";
    len--;
  }

  *ret_str = buffer;
  return 1;
}
