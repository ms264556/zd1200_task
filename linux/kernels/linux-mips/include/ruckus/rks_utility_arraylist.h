/*
 * Copyright 2016 Ruckus Wireless, Inc. All rights reserved.
 *
 * RUCKUS WIRELESS, INC. CONFIDENTIAL -
 * This is an unpublished, proprietary work of Ruckus Wireless, Inc., and is
 * fully protected under copyright and trade secret laws. You may not view,
 * use, disclose, copy, or distribute this file or any information contained
 * herein except pursuant to a valid license from Ruckus.
 */

struct arraylist_entry {
	int prev;
	int next;
};

struct arraylist {
	int size;
	int number;
	int head;
	struct arraylist_entry *entries;
};

static inline int arraylist_is_empty(const struct arraylist *arraylist)
{
	return (-1 == arraylist->head);
}

static inline int arraylist_get_size(const struct arraylist *arraylist)
{
	return arraylist->size;
}

static inline int arraylist_get_entries_number(const struct arraylist *arraylist)
{
	return arraylist->number;
}

static inline int arraylist_has_entry(const struct arraylist *arraylist, int idx)
{
	return (idx >= 0 && idx < arraylist->size && arraylist->entries[idx].next != -1);
}

static inline int arraylist_get_head(const struct arraylist *arraylist)
{
	return arraylist->head;
}

static inline int arraylist_get_tail(const struct arraylist *arraylist)
{
	return (-1 == arraylist->head) ? -1 : arraylist->entries[arraylist->head].prev;
}

int  arraylist_create_empty(struct arraylist *arraylist, int size);
int  arraylist_create_full(struct arraylist *arraylist, int size);
void arraylist_destroy(struct arraylist *arraylist);
void arraylist_insert_head(struct arraylist *arraylist, int idx);
void arraylist_insert_tail(struct arraylist *arraylist, int idx);
void arraylist_remove(struct arraylist *arraylist, int idx);
int  arraylist_remove_head(struct arraylist *arraylist);
int  arraylist_remove_tail(struct arraylist *arraylist);


#define arraylist_for_each(arraylist, n, idx) \
	for (n = (arraylist)->number, idx = (arraylist)->head; n > 0; n--, idx = (arraylist)->entries[idx].next)


#define arraylist_for_each_safe(arraylist, n, idx, next) \
	for (n = (arraylist)->number, idx = (arraylist)->head, next = ((-1 != idx) ? (arraylist)->entries[idx].next : -1) ; \
		n > 0; \
		n--, idx = next, next = ((-1 != idx) ? (arraylist)->entries[idx].next : -1))
