#ifndef __LIST__
#define __LIST__

typedef struct s_LIST_H {
    struct s_LIST_H *prev,*next;
}list_h_t;

void list_init(list_h_t *list);
void __list_del(list_h_t *prev, list_h_t *next);
void __list_add(list_h_t *nnode, list_h_t *prev, list_h_t *next);
void list_add(list_h_t *nnode, list_h_t *head);
void list_add_tail(list_h_t *nnode, list_h_t *head);
void __list_del_entry(list_h_t *entry);
void list_del(list_h_t *entry);
void list_move(list_h_t *list, list_h_t *head);
void list_move_tail(list_h_t *list, list_h_t *head);
int list_is_empty(const list_h_t *head);
int list_is_first(const list_h_t* list, const list_h_t* head);
int list_is_last(const list_h_t* list, const list_h_t* head);
int list_is_empty_careful(const list_h_t *head);


#define list_for_each(pos, head) for (pos = (head)->next; pos != (head); pos = pos->next)
/* 	一直for，直到到达head 
	keep doing for loop until head is reached */

#define list_for_each_head_dell(pos, head) for (pos = (head)->next; pos != (head); pos = (head)->next)
/* 	一直for，直到head的下一个等于head（在循环的过程中需要改变head，否则死循环） 
	keep doing for loop until "next" of head equals to head (head need to be changed during the loop, otherwise that will be a infinite loop) */

#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

/* list_t的指针减去它在整个结构体中的位置(&((type *)0)->member) (地址为0的type型数据中的member成员，就是member成员在type型数据中的偏移量），等于该结构体的开始
   The pointer of list_t minus its in the structure (&((type *)0)->member) (The member "member" in the data of type "type" at address 0, equals to the offset of member "member" in type "type"), you will get the start address of that structure*/
#define list_first_oneobj(head, o_type, o_member) list_entry((head)->next, o_type, o_member)

#define list_next_entry(pos, type, member) \
	list_entry((pos)->member.next, type, member)


#define list_prev_entry(pos, type, member) \
	list_entry((pos)->member.prev, type, member)

#endif