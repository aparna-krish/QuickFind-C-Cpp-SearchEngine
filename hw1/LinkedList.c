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

/* Code filled in and copyrighted 2025 by Aparna Krishnan*/

#include <stdio.h>
#include <stdlib.h>

#include "CSE333.h"
#include "LinkedList.h"
#include "LinkedList_priv.h"

// Takes in LinkedList and LinkedListNode, frees node from list and updates list
static void RemoveNode(LinkedList *list, LinkedListNode *node);


///////////////////////////////////////////////////////////////////////////////
// LinkedList implementation.

LinkedList* LinkedList_Allocate(void) {
  // Allocate the linked list record.
  LinkedList *ll = (LinkedList *) malloc(sizeof(LinkedList));
  Verify333(ll != NULL);

  // STEP 1: initialize the newly allocated record structure.
  ll -> num_elements = 0;
  ll -> head = NULL;
  ll -> tail = NULL;

  // Return our newly minted linked list.
  return ll;
}

void LinkedList_Free(LinkedList *list,
                     LLPayloadFreeFnPtr payload_free_function) {
  Verify333(list != NULL);
  Verify333(payload_free_function != NULL);

  // STEP 2: sweep through the list and free all of the nodes' payloads
  // (using the payload_free_function supplied as an argument) and
  // the nodes themselves.

  // Iterate through LinkedList
  while (list->head != NULL) {
    // Release payload before freeing nodes themselves
    payload_free_function(list -> head -> payload);
    LinkedListNode *temp = list -> head;
    list -> head = list -> head -> next;
    free(temp);
  }

  // free the LinkedList
  free(list);
}

int LinkedList_NumElements(LinkedList *list) {
  Verify333(list != NULL);
  return list->num_elements;
}

void LinkedList_Push(LinkedList *list, LLPayload_t payload) {
  Verify333(list != NULL);

  // Allocate space for the new node.
  LinkedListNode *ln = (LinkedListNode *) malloc(sizeof(LinkedListNode));
  Verify333(ln != NULL);

  // Set the payload
  ln->payload = payload;

  if (list->num_elements == 0) {
    // Degenerate case; list is currently empty
    Verify333(list->head == NULL);
    Verify333(list->tail == NULL);
    ln->next = ln->prev = NULL;
    list->head = list->tail = ln;
    list->num_elements = 1;
  } else {
    // STEP 3: typical case; list has >=1 elements
    ln -> next = list -> head;
    ln -> prev = NULL;
    list -> head = ln;
    ln -> next -> prev = ln;
    list -> num_elements++;
  }
}

bool LinkedList_Pop(LinkedList *list, LLPayload_t *payload_ptr) {
  Verify333(payload_ptr != NULL);
  Verify333(list != NULL);

  // STEP 4: implement LinkedList_Pop.  Make sure you test for
  // and empty list and fail.  If the list is non-empty, there
  // are two cases to consider: (a) a list with a single element in it
  // and (b) the general case of a list with >=2 elements in it.
  // Be sure to call free() to deallocate the memory that was
  // previously allocated by LinkedList_Push().

  // Check list isn't empty, return false if so
  if (list -> num_elements == 0) {
    return false;
  }

  // Store head and payload temporarily
  LinkedListNode *temp = list -> head;
  *payload_ptr = list -> head -> payload;

  // If it's last element in list, update head and tail to be NULL
  if (list -> num_elements == 1) {
    list -> head = NULL;
    list -> tail = NULL;
  } else {
    // Regular case, list head should now be next
    list -> head = list -> head -> next;
    // Make prev of new head NULL
    list->head->prev = NULL;
  }
  RemoveNode(list, temp);
  return true;
}

void LinkedList_Append(LinkedList *list, LLPayload_t payload) {
  Verify333(list != NULL);

  // STEP 5: implement LinkedList_Append.  It's kind of like
  // LinkedList_Push, but obviously you need to add to the end
  // instead of the beginning.

  // Allocate new node and set its payload
  LinkedListNode *ln = (LinkedListNode *) malloc(sizeof(LinkedListNode));
  Verify333(ln != NULL);
  ln -> payload = payload;

  // If this is first element in list, set it to list's head and tail
  if (list -> num_elements == 0) {
    ln -> next = NULL;
    ln -> prev = NULL;
    list -> head = ln;
    list -> tail = ln;
  } else {
    // If added to existing list, set as new tail and set old tail
    // to new tail prev
    ln -> prev = list -> tail;
    ln -> next = NULL;
    list -> tail -> next = ln;
    list -> tail = ln;
  }
  list -> num_elements++;
}

void LinkedList_Sort(LinkedList *list, bool ascending,
                     LLPayloadComparatorFnPtr comparator_function) {
  Verify333(list != NULL);
  if (list->num_elements < 2) {
    // No sorting needed.
    return;
  }

  // We'll implement bubblesort! Nnice and easy, and nice and slow :)
  int swapped;
  do {
    LinkedListNode *curnode;

    swapped = 0;
    curnode = list->head;
    while (curnode->next != NULL) {
      int compare_result = comparator_function(curnode->payload,
                                               curnode->next->payload);
      if (ascending) {
        compare_result *= -1;
      }
      if (compare_result < 0) {
        // Bubble-swap the payloads.
        LLPayload_t tmp;
        tmp = curnode->payload;
        curnode->payload = curnode->next->payload;
        curnode->next->payload = tmp;
        swapped = 1;
      }
      curnode = curnode->next;
    }
  } while (swapped);
}


///////////////////////////////////////////////////////////////////////////////
// LLIterator implementation.

LLIterator* LLIterator_Allocate(LinkedList *list) {
  Verify333(list != NULL);

  // OK, let's manufacture an iterator.
  LLIterator *li = (LLIterator *) malloc(sizeof(LLIterator));
  Verify333(li != NULL);

  // Set up the iterator.
  li->list = list;
  li->node = list->head;

  return li;
}

void LLIterator_Free(LLIterator *iter) {
  Verify333(iter != NULL);
  free(iter);
}

bool LLIterator_IsValid(LLIterator *iter) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);

  return (iter->node != NULL);
}

bool LLIterator_Next(LLIterator *iter) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);
  Verify333(iter->node != NULL);

  // STEP 6: try to advance iterator to the next node and return true if
  // you succeed, false otherwise
  // Note that if the iterator is already at the last node,
  // you should move the iterator past the end of the list

  iter -> node = iter -> node -> next;
  return (iter -> node != NULL);
}

void LLIterator_Get(LLIterator *iter, LLPayload_t *payload) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);
  Verify333(iter->node != NULL);

  *payload = iter->node->payload;
}

bool LLIterator_Remove(LLIterator *iter,
                       LLPayloadFreeFnPtr payload_free_function) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);
  Verify333(iter->node != NULL);

  // STEP 7: implement LLIterator_Remove.  This is the most
  // complex function you'll build.  There are several cases
  // to consider:
  // - degenerate case: the list becomes empty after deleting.
  // - degenerate case: iter points at head
  // - degenerate case: iter points at tail
  // - fully general case: iter points in the middle of a list,
  //                       and you have to "splice".
  //
  // Be sure to call the payload_free_function to free the payload
  // the iterator is pointing to, and also free any LinkedList
  // data structure element as appropriate.


  LinkedList *list = iter -> list;
  LinkedListNode *old = iter -> node;

  // If last element left, make list NULL
  if (list -> num_elements == 1) {
    payload_free_function(old -> payload);
    RemoveNode(list, old);
    list -> head = NULL;
    list -> tail = NULL;
    iter -> node = NULL;
    return false;
  }

  // If first element, set next element to new head and advance iter
  if (old == list -> head) {
    list -> head = old -> next;
    list -> head -> prev = NULL;
    iter -> node = list -> head;
  } else if (old == list -> tail) {
    // If last element, set prev element to new tail shift iter back
    list -> tail = old -> prev;
    list -> tail -> next = NULL;
    iter -> node = list -> tail;
  } else {
    // Point old prev and old next to each other and advance iter
    old -> prev -> next = old -> next;
    old -> next -> prev = old -> prev;
    iter -> node = old -> next;
  }
  // Free payload and de-allocate old node
  payload_free_function(old -> payload);
  RemoveNode(list, old);
  return true;
}


///////////////////////////////////////////////////////////////////////////////
// Helper functions

bool LLSlice(LinkedList *list, LLPayload_t *payload_ptr) {
  Verify333(payload_ptr != NULL);
  Verify333(list != NULL);

  // STEP 8: implement LLSlice.
  if (list -> num_elements == 0) {
    return false;
  }

  // Assign old tail and payload
  LinkedListNode *old_tail = list -> tail;
  *payload_ptr = list -> tail -> payload;

  // If last element left, make list NULL
  if (list -> num_elements == 1) {
    list -> head = NULL;
    list -> tail = NULL;
  } else {
    list -> tail = old_tail -> prev;
    list -> tail -> next = NULL;
  }
  RemoveNode(list, old_tail);
  return true;
}

void LLIteratorRewind(LLIterator *iter) {
  iter->node = iter->list->head;
}

static void RemoveNode(LinkedList *list, LinkedListNode *node) {
  // Decrement num_elements and free memory
  list -> num_elements--;
  free(node);
}
