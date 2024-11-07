#include "./include/list.h"

void list_init(list_h_t *list)
{
	list->prev = list;
	list->next = list;
	return;
}

void __list_del(list_h_t *prev, list_h_t *next)
{
	next->prev = prev;
	prev->next = next;
	return;
}

void __list_add(list_h_t *nnode, list_h_t *prev, list_h_t *next)
{
	next->prev = nnode;
	nnode->next = next;
	nnode->prev = prev;
	prev->next = nnode;
	return;
}

void list_add(list_h_t *nnode, list_h_t *head)
{
	__list_add(nnode, head, head->next);
	return;
}

void list_add_tail(list_h_t *nnode, list_h_t *head)
{
	__list_add(nnode, head->prev, head);
	return;
}

void __list_del_entry(list_h_t *entry)
{
	__list_del(entry->prev, entry->next);
	return;
}

void list_del(list_h_t *entry)
{
	__list_del(entry->prev, entry->next);
	list_init(entry);
	return;
}

void list_move(list_h_t *list, list_h_t *head)
{
	list_del(list);
	list_add(list, head);
	return;
}
void list_move_tail(list_h_t *list, list_h_t *head)
{
	list_del(list);
	list_add_tail(list, head);
	return;
}

int list_is_empty(const list_h_t *head)
{
	if (head->next == head)
	{
		return 1;
	}
	return 0;
}

int list_is_first(const list_h_t* list, const list_h_t* head)
{
	if(list->prev == head)
	{
		return 1;
	}
	return 0;
}

int list_is_last(const list_h_t* list, const list_h_t* head)
{
	if(list->next == head)
	{
		return 1;
	}
	return 0;
}

int list_is_empty_careful(const list_h_t *head)
{
	list_h_t *next = head->next;
	if (next == head && next == head->prev)
	{
		return 1;
	}
	return 0;
}