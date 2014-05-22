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

List *List_create(void *pool){
	return _calloc_box(pool);
}

void List_destroy(List *list, void *pool){
	LIST_FOREACH(list, first, next, cur){
		if (cur->prev){
			_free_box(pool, cur->prev);
		}
	}
	_free_box(pool, list->last);
	_free_box(pool, list);
}

void List_clear(List *list, void *pool){
	LIST_FOREACH(list, first, next, cur){
		_free_box(pool, cur->value);
	}
}

void List_clear_destroy(List *list){
	List_clear(list);
	List_destroy(list);
	// TODO should make this more efficient
	// clear/delete node by node instead of looping twice
}

void List_push(List *list, void *value, void *pool){
	ListNode *node = _calloc_box(pool)
}
