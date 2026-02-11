/**
 * @file test_vec.c
 * @brief Vector tests (placeholder)
 */

#include "test_utils.h"
#include "core/dc_vec.h"

int test_vec_main(void) {
    TEST_SUITE_BEGIN("Vector Tests");

    dc_vec_t vec;
    TEST_ASSERT_EQ(DC_OK, dc_vec_init(&vec, sizeof(int)), "vec init");
    TEST_ASSERT_EQ(0u, dc_vec_length(&vec), "vec length empty");

    int v1 = 1, v2 = 2, v3 = 3;
    TEST_ASSERT_EQ(DC_OK, dc_vec_push(&vec, &v1), "push 1");
    TEST_ASSERT_EQ(DC_OK, dc_vec_push(&vec, &v2), "push 2");
    TEST_ASSERT_EQ(DC_OK, dc_vec_push(&vec, &v3), "push 3");
    TEST_ASSERT_EQ(3u, dc_vec_length(&vec), "vec length after push");

    int out = 0;
    TEST_ASSERT_EQ(DC_OK, dc_vec_get(&vec, (size_t)1, &out), "get index 1");
    TEST_ASSERT_EQ(2, out, "value at index 1");

    int v42 = 42;
    TEST_ASSERT_EQ(DC_OK, dc_vec_set(&vec, (size_t)1, &v42), "set index 1");
    TEST_ASSERT_EQ(DC_OK, dc_vec_get(&vec, (size_t)1, &out), "get index 1 after set");
    TEST_ASSERT_EQ(42, out, "value after set");

    int v7 = 7;
    TEST_ASSERT_EQ(DC_OK, dc_vec_insert(&vec, (size_t)1, &v7), "insert at index 1");
    TEST_ASSERT_EQ(4u, dc_vec_length(&vec), "length after insert");
    TEST_ASSERT_EQ(DC_OK, dc_vec_get(&vec, (size_t)1, &out), "get inserted");
    TEST_ASSERT_EQ(7, out, "inserted value");

    TEST_ASSERT_EQ(DC_OK, dc_vec_remove(&vec, (size_t)2, &out), "remove index 2");
    TEST_ASSERT_EQ(42, out, "removed value");
    TEST_ASSERT_EQ(3u, dc_vec_length(&vec), "length after remove");

    out = 0;
    TEST_ASSERT_EQ(DC_OK, dc_vec_pop(&vec, &out), "pop");
    TEST_ASSERT_EQ(3, out, "pop value");
    TEST_ASSERT_EQ(2u, dc_vec_length(&vec), "length after pop");

    TEST_ASSERT_EQ(DC_OK, dc_vec_resize(&vec, (size_t)4), "resize up");
    TEST_ASSERT_EQ(4u, dc_vec_length(&vec), "length after resize");
    TEST_ASSERT_EQ(DC_OK, dc_vec_get(&vec, (size_t)2, &out), "get zero-init index 2");
    TEST_ASSERT_EQ(0, out, "zero-init value index 2");

    size_t idx = 0;
    TEST_ASSERT_EQ(DC_OK, dc_vec_find(&vec, &v7, NULL, &idx), "find value");
    TEST_ASSERT_EQ(1u, idx, "find index");

    int v9 = 9;
    TEST_ASSERT_EQ(DC_OK, dc_vec_insert_unordered(&vec, (size_t)1, &v9), "insert unordered index 1");
    TEST_ASSERT_EQ(5u, dc_vec_length(&vec), "length after unordered insert");
    TEST_ASSERT_EQ(DC_OK, dc_vec_get(&vec, (size_t)1, &out), "get unordered inserted");
    TEST_ASSERT_EQ(9, out, "unordered inserted value");
    TEST_ASSERT_EQ(DC_OK, dc_vec_get(&vec, (size_t)4, &out), "get displaced value at end");
    TEST_ASSERT_EQ(7, out, "displaced moved to end");

    TEST_ASSERT_EQ(DC_OK, dc_vec_remove_unordered(&vec, (size_t)0, &out), "remove unordered index 0");
    TEST_ASSERT_EQ(1, out, "remove unordered removed value");
    TEST_ASSERT_EQ(4u, dc_vec_length(&vec), "length after unordered remove");
    TEST_ASSERT_EQ(DC_OK, dc_vec_get(&vec, (size_t)0, &out), "get swapped-in value at index 0");
    TEST_ASSERT_EQ(7, out, "last moved to removed slot");

    TEST_ASSERT_EQ(DC_OK, dc_vec_swap_remove(&vec, (size_t)1, &out), "swap remove index 1");
    TEST_ASSERT_EQ(9, out, "swap remove removed value");
    TEST_ASSERT_EQ(3u, dc_vec_length(&vec), "length after swap remove");

    dc_vec_free(&vec);

    dc_vec_t empty;
    TEST_ASSERT_EQ(DC_OK, dc_vec_init(&empty, sizeof(int)), "init empty for pop");
    TEST_ASSERT_EQ(DC_ERROR_NOT_FOUND, dc_vec_pop(&empty, &out), "pop empty");
    dc_vec_free(&empty);

    TEST_SUITE_END("Vector Tests");
}
