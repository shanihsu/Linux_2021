#include <assert.h>
#include <stdlib.h>
#include <linux/string.h>

#include "common.h"

static uint16_t values[256];

__attribute__((nonnull(1,2)))
static struct list_head *merge(
				struct list_head *a, struct list_head *b)
{
	struct list_head *head, **tail = &head;

	for (;;) {
		struct listitem *item = NULL;
		uint16_t value_a = list_entry(a, __typeof__(*item), list)->i;
		uint16_t value_b = list_entry(b, __typeof__(*item), list)->i;
		/* if equal, take 'a' -- important for sort stability */
		if (cmpint(&value_a, &value_b) <= 0) {
			*tail = a;
			tail = &a->next;
			a = a->next;
			if (!a) {
				*tail = b;
				break;
			}
		} else {
			*tail = b;
			tail = &b->next;
			b = b->next;
			if (!b) {
				*tail = a;
				break;
			}
		}
	}
	return head;
}

__attribute__((nonnull(1,2,3)))
static void merge_final(struct list_head *head,
			struct list_head *a, struct list_head *b)
{
	struct list_head *tail = head;
	uint8_t count = 0;

	for (;;) {
		struct listitem *item = NULL;
		uint16_t value_a = list_entry(a, __typeof__(*item), list)->i;
		uint16_t value_b = list_entry(b, __typeof__(*item), list)->i;
		/* if equal, take 'a' -- important for sort stability */
		if (cmpint(&value_a, &value_b) <= 0) {
			tail->next = a;
			a->prev = tail;
			tail = a;
			a = a->next;
			if (!a)
				break;
		} else {
			tail->next = b;
			b->prev = tail;
			tail = b;
			b = b->next;
			if (!b) {
				b = a;
				break;
			}
		}
	}

	/* Finish linking remainder of list b on to tail */
	tail->next = b;
	do {
		//if (unlikely(!++count))
		if(!++count){
			struct listitem *item = NULL;
			uint16_t value_b = list_entry(b, __typeof__(*item), list)->i;
			cmpint(&value_b, &value_b);
		}
		b->prev = tail;
		tail = b;
		b = b->next;
	} while (b);

	/* And the final links to make a circular doubly-linked list */
	tail->next = head;
	head->prev = tail;
}

__attribute__((nonnull(1)))
void list_sort(struct list_head *head)
{
	struct list_head *list = head->next, *pending = NULL;
	size_t count = 0;	/* Count of pending */

	if (list == head->prev)	/* Zero or one elements */
		return;

	/* Convert to a null-terminated singly-linked list. */
	head->prev->next = NULL;

	do {
		size_t bits;
		struct list_head **tail = &pending;

		/* Find the least-significant clear bit in count */
		for (bits = count; bits & 1; bits >>= 1)
			tail = &(*tail)->prev;
		/* Do the indicated merge */
		//if (likely(bits)) {
		if(bits){
			struct list_head *a = *tail, *b = a->prev;

			a = merge(b, a);
			/* Install the merged result in place of the inputs */
			a->prev = b->prev;
			*tail = a;
		}

		/* Move one element from input list to pending */
		list->prev = pending;
		pending = list;
		list = list->next;
		pending->next = NULL;
		count++;
	} while (list);

	/* End of input; merge together all the pending lists. */
	list = pending;
	pending = pending->prev;
	for (;;) {
		struct list_head *next = pending->prev;

		if (!next)
			break;
		list = merge(pending, list);
		pending = next;
	}
	/* The final merge, rebuilding prev links */
	merge_final(head, pending, list);
}
//EXPORT_SYMBOL(list_sort);


int main(void)
{
    struct list_head testlist;
    struct listitem *item = NULL, *is = NULL;
    size_t i;

    random_shuffle_array(values, (uint16_t) ARRAY_SIZE(values));

    INIT_LIST_HEAD(&testlist);

    assert(list_empty(&testlist));

    for (i = 0; i < ARRAY_SIZE(values); i++) {
        item = (struct listitem *) malloc(sizeof(*item));
        assert(item);
        item->i = values[i];
        list_add_tail(&item->list, &testlist);
    }

    assert(!list_empty(&testlist));

    qsort(values, ARRAY_SIZE(values), sizeof(values[0]), cmpint);
    list_sort(&testlist);

    i = 0;
    list_for_each_entry_safe (item, is, &testlist, list) {
        assert(item->i == values[i]);
        list_del(&item->list);
        free(item);
        i++;
    }

    assert(i == ARRAY_SIZE(values));
    assert(list_empty(&testlist));

    return 0;
}