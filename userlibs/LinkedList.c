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
void List_init(List *list){
	uint32_t i;
	if(list->startAddr != NULL){
		for(i = 0; i < list->count; i++){
			ListNode *newnode = (ListNode *)(list->startAddr + i*sizeof(ListNode));
			List_push(list, newnode);
		}
	}
}

ListNode *List_remove(List *list, ListNode *node){
	ListNode *result = NULL;
	if(list->first != NULL && list->last != NULL && node != NULL){
		if(node == list->first && node == list->last){
			list->first = NULL;
			list->last = NULL;
		} else if(node == list->first){
			list->first = node->next; // list->first->next;
			list->first->prev = NULL;
		} else if(node == list-> last){
			list->last = node->prev;
			list->last->next = NULL;
		} else {
			ListNode *after = node->next;
			ListNode *before = node->prev;
			after->prev = before;
			before->next = after;
		}
		
		list->count--;
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
