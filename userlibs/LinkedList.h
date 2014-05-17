/*-----------------------------------------------------------------------------
 * Name:    LinkedList.h
 * Purpose: Provide a library of definitions and utilities for linked lists in C
 *-----------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/

#ifndef __LNKLST_H
#define __LNKLST_H

#include <stdint.h>

// definition so that we can reference this struct later (in node construction)
struct ListNode;

typedef struct ListNode {
	struct ListNode *next;	// next element in list
	struct ListNode *prev;	// previous element in list
	void *value;						// value to be stored, type TBD
} ListNode;

typedef struct List {
	uint32_t count;			// # elements in list, always updated
	ListNode *first;		// head of list
	ListNode *last;			// tail of list
} List;

List *List_create(void *pool);	// have to hand it the pool because of how the OS memory management macros work
void List_destroy(List *list, void *pool);
void List_clear(List *list);
void List_clear_destroy(List *list);

// macros for easy info grab
#define List_count(A) ((A)->count)
#define List_first(A) ((A)->first != NULL ? (A)->first->value : NULL)
#define List_last(A) ((A)->last != NULL ? (A)->last->value : NULL)

void List_push(List *list, void *value);
void *List_pop(List *list);

void List_unshift(List *list, void *value);
void *List_shift(List *list);

void *List_remove(List *list, ListNode *node);

#define LIST_FOREACH(L, S, M, V) ListNode *_node = NULL;\
		ListNode *V = NULL;\
    for(V = _node = L->S; _node != NULL; V = _node = _node->M)



#endif /* __LNKLST_H */
