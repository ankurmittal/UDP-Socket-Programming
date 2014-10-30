#include <stdio.h>
#include <stdlib.h>

typedef struct ll_Node
{
	void *data;
	struct ll_Node *next;
	struct ll_Node *prev;
}ll_node;

ll_node* ll_initiate(void *data);
void ll_insert(ll_node *ll_pointer, void *data);
void ll_insertAt(ll_node *ll_pointer, void *data, int index);
void ll_insertAfter(ll_node *ll_pointer, void *data, ll_node *afterThis);
void* ll_delete(ll_node *ll_pointer, ll_node *toDelete);
void ll_destroy(ll_node *ll_pointer);
