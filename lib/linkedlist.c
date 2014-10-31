#include "linkedlist.h"

ll_node* ll_initiate(void *data) {
	ll_node *ll_pointer;
	printf("\nInitiating linked list now..!!\n");
	ll_pointer = (ll_node *)malloc(sizeof(ll_node));
	ll_pointer->next = NULL;
	ll_pointer->prev = NULL;
	ll_pointer->data = data;
	return ll_pointer;
}

void ll_insert(ll_node *ll_pointer, void *data)
{
	printf("\nInserting in Linked List now..!!\n");
	while(ll_pointer->next!=NULL)
	{
		ll_pointer = ll_pointer -> next;
	}
	ll_pointer->next = (ll_node *)malloc(sizeof(ll_node));
	(ll_pointer->next)->prev = ll_pointer;
	ll_pointer = ll_pointer->next;
	ll_pointer->data = data;
	ll_pointer->next = NULL;
}

void ll_insertAt(ll_node *ll_pointer, void *data, int index)
{
	int i=0;
	ll_node *temp;
	temp = ll_pointer;
	while(ll_pointer->next!=NULL && i<index)
		ll_pointer = ll_pointer->next;
	
	if(i!=index) {
		printf("No such index available..!!\n");
		return;
	}
	if(ll_pointer->next == NULL) {
		ll_insert(temp, data);
		return;
	}
	ll_insertAfter(temp, data, ll_pointer);
}

void ll_insertAfter(ll_node *ll_pointer, void *data, ll_node *afterThis)
{
	ll_node *temp;
	while(ll_pointer->next!=NULL && ll_pointer->next != afterThis)
		ll_pointer = ll_pointer->next;

	temp = ll_pointer->next;
	ll_pointer->next = (ll_node *)malloc(sizeof(ll_node));
	(ll_pointer->next)->prev = ll_pointer;
	ll_pointer = ll_pointer->next;
	ll_pointer->data = data;
	ll_pointer->next = temp;
	temp->prev = ll_pointer;
}

void* ll_delete(ll_node *ll_pointer, ll_node *toDelete)
{
	void *data;
	ll_node *temp;

	while(ll_pointer->next!=NULL && ll_pointer->next != toDelete)
		ll_pointer = ll_pointer -> next;

	if(ll_pointer->next==NULL) {
		printf("Element is not present in the list\n");
		return NULL;
	}
	temp = ll_pointer -> next;
	ll_pointer->next = temp->next;
	temp->prev =  ll_pointer;
	data = temp->data;
	free(temp);
	return data;
}

void ll_destroy(ll_node *ll_pointer) {
	ll_node *temp;
	while(ll_pointer->next !=NULL) {
		temp = ll_pointer->next;
		free(ll_pointer);
		ll_destroy(temp);
	}
}
