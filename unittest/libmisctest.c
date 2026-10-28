/* Regression tests for miscellaneous utilities. */

#include "tinytest.h"
#include "queue.h"
#include "cocotypes.h"

static void check_delete_position(int count, int position)
{
	NodeType head = NULL;
	NodeType target, node;
	int values[] = {10, 20, 30};
	int remaining[3];
	int i, ec, remaining_count = 0;
	int terminated;

	for (i = 0; i < count; i++)
	{
		ec = qAddNode(&head, &values[i], sizeof(values[i]));
		ASSERT_EQUALS(0, ec);
	}

	target = head;
	for (i = 0; i < position; i++)
	{
		target = qGetNextNode(target);
	}
	ec = qDeleteNode(&head, target);

	/* Save the remaining sequence and release the queue before asserting. */
	for (node = head; node != NULL && remaining_count < 3;
	     node = qGetNextNode(node))
	{
		remaining[remaining_count++] = *(int *)node->data;
	}
	terminated = node == NULL;
	while (head != NULL)
	{
		qDeleteLastNode(&head);
	}

	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(count - 1, remaining_count);
	ASSERT("remaining queue must terminate", terminated);
	for (i = 0; i < remaining_count; i++)
	{
		ASSERT_EQUALS(values[i < position ? i : i + 1], remaining[i]);
	}
}

static void test_delete_tail(void)
{
	check_delete_position(3, 2);
}

static void test_delete_head(void)
{
	check_delete_position(3, 0);
}

static void test_delete_middle(void)
{
	check_delete_position(3, 1);
}

static void test_delete_only_node(void)
{
	check_delete_position(1, 0);
}

static void test_int4_unsigned_values(void)
{
	u_char bytes[][4] = {
		{0x00, 0x00, 0x00, 0x00},
		{0x12, 0x34, 0x56, 0x78},
		{0x7f, 0xff, 0xff, 0xff},
		{0x80, 0x00, 0x00, 0x00},
		{0x89, 0xab, 0xcd, 0xef},
		{0xff, 0xff, 0xff, 0xff}
	};
	unsigned int expected[] = {
		0x00000000U, 0x12345678U, 0x7fffffffU,
		0x80000000U, 0x89abcdefU, 0xffffffffU
	};
	unsigned int actual;
	size_t i;

	for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
	{
		actual = int4(bytes[i]);
		ASSERT_EQUALS(expected[i], actual);
	}
}

int main(void)
{
	RUN(test_delete_tail);
	RUN(test_delete_head);
	RUN(test_delete_middle);
	RUN(test_delete_only_node);
	RUN(test_int4_unsigned_values);

	return TEST_REPORT();
}
