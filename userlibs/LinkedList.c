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
