/*
 * Copyright 2016 Ruckus Wireless, Inc. All rights reserved.
 *
 * RUCKUS WIRELESS, INC. CONFIDENTIAL -
 * This is an unpublished, proprietary work of Ruckus Wireless, Inc., and is
 * fully protected under copyright and trade secret laws. You may not view,
 * use, disclose, copy, or distribute this file or any information contained
 * herein except pursuant to a valid license from Ruckus.
 */

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/hardirq.h>
#include <linux/module.h>
#endif

#include <ruckus/rks_utility_arraylist.h>

int arraylist_create_empty(struct arraylist *arraylist, int size)
{
	int idx;

	if (size <= 0) {
		return -1;
	}


	arraylist->entries = kmalloc(size * sizeof(struct arraylist_entry), in_atomic() ? GFP_ATOMIC : GFP_KERNEL);
	if (NULL == arraylist->entries) {
		return -1;
	}
	for (idx = 0; idx < size; idx++) {
		arraylist->entries[idx].prev = arraylist->entries[idx].next = -1;
	}
	arraylist->size   = size;
	arraylist->number = 0;
	arraylist->head   = -1;
	return 0;
}
EXPORT_SYMBOL(arraylist_create_empty);

int arraylist_create_full(struct arraylist *arraylist, int size)
{
	int idx;

	arraylist->entries = kmalloc(size * sizeof(struct arraylist_entry), in_atomic() ? GFP_ATOMIC : GFP_KERNEL);
	if (NULL == arraylist->entries) {
		return -1;
	}
	for (idx = 0; idx < size - 1; idx++) {
		arraylist->entries[idx].next = idx + 1;
	}
	arraylist->entries[size - 1].next = 0;
	arraylist->entries[0].prev = size - 1;
	for (idx = 1; idx < size; idx++) {
		arraylist->entries[idx].prev = idx - 1;
	}
	arraylist->size   = size;
	arraylist->number = size;
	arraylist->head   = 0;
	return 0;
}
EXPORT_SYMBOL(arraylist_create_full);

void arraylist_destroy(struct arraylist *arraylist)
{
	if (NULL == arraylist->entries) {
		kfree(arraylist->entries);
		arraylist->entries = NULL;
	}
	arraylist->size	   = 0;
	arraylist->number  = 0;
	arraylist->head	   = -1;
}
EXPORT_SYMBOL(arraylist_destroy);

void arraylist_insert_head(struct arraylist *arraylist, int idx)
{
	if (idx >= arraylist->size || idx < 0) {
		return;
	}

	if (arraylist->entries[idx].next != -1) {
		arraylist_remove(arraylist, idx);
	}

	if (-1 == arraylist->head) {
		arraylist->entries[idx].prev = arraylist->entries[idx].next = idx;
	} else {
		arraylist->entries[idx].next = arraylist->head;
		arraylist->entries[idx].prev = arraylist->entries[arraylist->head].prev;
		arraylist->entries[arraylist->entries[arraylist->head].prev].next = idx;
		arraylist->entries[arraylist->head].prev = idx;
	}
	arraylist->head = idx;
	arraylist->number++;
	return;
}
EXPORT_SYMBOL(arraylist_insert_head);

void arraylist_insert_tail(struct arraylist *arraylist, int idx)
{
	if (idx >= arraylist->size || idx < 0) {
		return;
	}

	if (-1 != arraylist->entries[idx].next) {
		arraylist_remove(arraylist, idx);
	}

	if (-1 == arraylist->head) {
		arraylist->entries[idx].prev = arraylist->entries[idx].next = idx;
		arraylist->head = idx;
	} else {
		arraylist->entries[arraylist->entries[arraylist->head].prev].next = idx;
		arraylist->entries[idx].next = arraylist->head;
		arraylist->entries[idx].prev = arraylist->entries[arraylist->head].prev;
		arraylist->entries[arraylist->head].prev = idx;
	}
	arraylist->number++;
	return;
}
EXPORT_SYMBOL(arraylist_insert_tail);

void arraylist_remove(struct arraylist *arraylist, int idx)
{
	int prev, next;

	if (idx >= arraylist->size || idx < 0) {
		return;
	}

	prev = arraylist->entries[idx].prev;
	next = arraylist->entries[idx].next;

	if (-1 == prev || -1 == next) {
		return;
	}

	if (next == idx) {
		arraylist->head = -1;
	} else {
		arraylist->entries[prev].next = next;
		arraylist->entries[next].prev = prev;
		if (arraylist->head == idx) {
			arraylist->head = next;
		}
	}
	arraylist->number--;
	arraylist->entries[idx].prev = arraylist->entries[idx].next = -1;
}
EXPORT_SYMBOL(arraylist_remove);

int arraylist_remove_head(struct arraylist *arraylist)
{
	int idx;

	if (-1 != (idx = arraylist_get_head(arraylist))) {
		arraylist_remove(arraylist, idx);
	}
	return idx;
}
EXPORT_SYMBOL(arraylist_remove_head);

int arraylist_remove_tail(struct arraylist *arraylist)
{
	int idx;

	if (-1 != (idx = arraylist_get_tail(arraylist))) {
		arraylist_remove(arraylist, idx);
	}
	return idx;
}
EXPORT_SYMBOL(arraylist_remove_tail);
