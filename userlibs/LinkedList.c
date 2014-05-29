/*------------------------------------------------------------------------------
 *   
 *------------------------------------------------------------------------------
 *      Name:    LinkedList.c
 *      Purpose: Linked List function library for C
 *      Note(s): 
 *------------------------------------------------------------------------------
 *      
 *----------------------------------------------------------------------------*/
 
#include <stm32f2xx.h>
#include "dbg.h"
#include "LinkedList.h"
#include <rtl.h>

// ONLY EVER USE THIS ONCE OR ELSE YOU LOSE ALL YOUR DATA.
// This function is only to be used to initialize the 
// "free space" lists ONCE, and ONLY ONCE, before the OS starts.
// TODO: should probably sanitize the data fields in the 
// nodes to keep from getting ghost data
void List_init(List *list){
	uint32_t i;
	if(list->startAddr != NULL){	// starting address of the list, which are 
																// the SRAM offsets in this specific case.
		for(i = 0; i < list->count; i++){	// push "new" nodes til we hit the size limit
			ListNode *newnode = (ListNode *)(list->startAddr + i*sizeof(ListNode));
			List_push(list, newnode);
			list->count--;	// push will increment the list count, have to 
											// decrement it in order to not loop infinitely
		}
	}
}

ListNode *List_remove(List *list, ListNode *node){
	ListNode *result = NULL;
	// Check if the list isn't empty, and if the node we're trying
	// to move actually exists.  If either of those things happens,
	// we return NULL.  The way that the lists get constructed, 
	// *first and *last are either both NULL or both valid (even
	// if it's the same element in both, 1-element list)
	if(list->first != NULL && list->last != NULL && node != NULL){
		// if the node is the only node in the list
		if(node == list->first && node == list->last){
			list->first = NULL;
			list->last = NULL;
		} else if(node == list->first){	// if it's the first element in the list
			list->first = node->next; 		// list->first->next;
			list->first->prev = NULL;
		} else if(node == list-> last){	// if it's the last element in the list
			list->last = node->prev;
			list->last->next = NULL;
		} else {												// if it's in the middle of the list.
			ListNode *after = node->next;
			ListNode *before = node->prev;
			after->prev = before;
			before->next = after;
		}
		// only decrement count if the remove is successful
		list->count--;
		// going to return our node in question
		result = node;
	}
	return result;
}

// push and pop manipulate "last" direction or from tail
void List_push(List *list, ListNode *node){
	if(list->last == NULL){
		list->first = node;
		list->last = node;
	} else {
		list->last->next = node;
		node->prev = list->last;
		list->last = node;
	}
	list->count++;
}

ListNode *List_pop(List *list){
	ListNode *node = list->last;
	return node != NULL ? List_remove(list, node) : NULL;
}

// shift and unshift manipulate the "first" or head
void List_unshift(List *list, ListNode *node){
	if(list->first == NULL){
		list->last = node;
		list->first = node;
	} else {
		list->first->prev = node;
		node->next = list->first;
		list->first = node;
	}
	list->count++;
}

ListNode *List_shift(List *list){
	ListNode *node = list->first;
	return node != NULL ? List_remove(list, node) : NULL;
}
