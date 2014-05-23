/*-----------------------------------------------------------------------------
 * Name:    LinkedList.h
 * Purpose: Provide a library of definitions and utilities for linked lists in C
 *-----------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/

#ifndef __LNKLST_H
#define __LNKLST_H

#include <stdint.h>
#include "..\TextMessage.h"

// definition so that we can reference this struct later (in node construction)
// struct ListNode;
typedef struct _NodeData {
	uint8_t text[160];			// text to be stored
	uint8_t cnt;						// how many items are in this message
	uint8_t sub;						// if the text is broken up, increment this to show a "1/2, 2/2" type deal later on
	Timestamp time;					// timestamp struct so we minimize data parsing between functions
} NodeData;

typedef struct _ListNode {
	struct _ListNode *next;	// next element in list
	struct _ListNode *prev;	// previous element in list
	NodeData data;
} ListNode;

typedef struct _List {
	uint32_t count;			// # elements in list, always updated
	ListNode *first;		// head of list
	ListNode *last;			// tail of list
} List;

//List *List_create(void);	// we don't need this.
void List_destroy(List *list);
void List_clear(List *list);
void List_clear_destroy(List *list);

// macros for easy info grab
#define List_count(A) ((A)->count)
// these get the values held in the first or last list element.  Marginally useful in this application.
#define List_first(A) ((A)->first != NULL ? (A)->first->data : NULL)
#define List_last(A) ((A)->last != NULL ? (A)->last->data : NULL)


void List_push(List *list, NodeData *data);
void *List_pop(List *list);

void List_unshift(List *list, void *value);
void *List_shift(List *list);

void *List_remove(List *list, ListNode *node);

#define LIST_FOREACH(List, First, Next, Cur) ListNode *_node = NULL;\
		ListNode *Cur = NULL;\
    for(Cur = _node = List->First; _node != NULL; Cur = _node = _node->Next)


#endif /* __LNKLST_H */
