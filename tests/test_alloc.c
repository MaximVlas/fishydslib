/**
 * @file test_alloc.c
 * @brief Memory allocation tests
 */

#include "test_utils.h"
#include "core/dc_alloc.h"

static size_t hook_alloc_calls = 0;
static size_t hook_realloc_calls = 0;
static size_t hook_free_calls = 0;

static void* hook_alloc(size_t size) {
    hook_alloc_calls++;
    return malloc(size);
}

static void* hook_realloc(void* ptr, size_t size) {
    hook_realloc_calls++;
    return realloc(ptr, size);
}

static void hook_free(void* ptr) {
    hook_free_calls++;
    free(ptr);
}

void test_basic_allocation(void) {
    void* ptr = dc_alloc((size_t)100);
    TEST_ASSERT_NOT_NULL(ptr, "dc_alloc returns non-NULL");
    dc_free(ptr);
    
    ptr = dc_alloc((size_t)0);
    TEST_ASSERT_NULL(ptr, "dc_alloc(0) returns NULL");
    
    ptr = dc_calloc((size_t)10, (size_t)10);
    TEST_ASSERT_NOT_NULL(ptr, "dc_calloc returns non-NULL");
    dc_free(ptr);
    
    ptr = dc_calloc((size_t)0, (size_t)10);
    TEST_ASSERT_NULL(ptr, "dc_calloc(0, 10) returns NULL");
}

void test_string_duplication(void) {
    const char* test_str = "Hello, World!";
    char* dup = dc_strdup(test_str);
    TEST_ASSERT_NOT_NULL(dup, "dc_strdup returns non-NULL");
    TEST_ASSERT_STR_EQ(test_str, dup, "dc_strdup copies correctly");
    dc_free(dup);
    
    dup = dc_strdup(NULL);
    TEST_ASSERT_NULL(dup, "dc_strdup(NULL) returns NULL");
    
    dup = dc_strndup(test_str, (size_t)5);
    TEST_ASSERT_NOT_NULL(dup, "dc_strndup returns non-NULL");
    TEST_ASSERT_STR_EQ("Hello", dup, "dc_strndup copies correctly");
    dc_free(dup);
}

void test_safe_free(void) {
    dc_free(NULL); /* Should not crash */
    TEST_ASSERT(1, "dc_free(NULL) is safe");
}

void test_realloc_behavior(void) {
    char* ptr = (char*)dc_alloc((size_t)4);
    TEST_ASSERT_NOT_NULL(ptr, "dc_alloc for realloc test");
    if (ptr) {
        ptr[0] = 'A';
        ptr[1] = 'B';
        ptr[2] = 'C';
        ptr[3] = 'D';
    }

    char* grown = (char*)dc_realloc(ptr, (size_t)8);
    TEST_ASSERT_NOT_NULL(grown, "dc_realloc grow");
    if (grown) {
        TEST_ASSERT(grown[0] == 'A', "realloc preserves data");
        dc_free(grown);
    }

    char* alloc_from_null = (char*)dc_realloc(NULL, (size_t)16);
    TEST_ASSERT_NOT_NULL(alloc_from_null, "dc_realloc(NULL, size) allocates");
    dc_free(alloc_from_null);

    void* freed = dc_realloc(dc_alloc((size_t)8), (size_t)0);
    TEST_ASSERT_NULL(freed, "dc_realloc(size=0) returns NULL");
}

void test_alloc_hooks(void) {
    hook_alloc_calls = 0;
    hook_realloc_calls = 0;
    hook_free_calls = 0;

    dc_alloc_hooks_t hooks = {
        .alloc = hook_alloc,
        .realloc = hook_realloc,
        .free = hook_free
    };

    TEST_ASSERT_EQ(DC_OK, dc_alloc_set_hooks(&hooks), "set hooks");

    void* ptr = dc_alloc((size_t)32);
    TEST_ASSERT_NOT_NULL(ptr, "hook alloc");
    ptr = dc_realloc(ptr, (size_t)64);
    TEST_ASSERT_NOT_NULL(ptr, "hook realloc");
    dc_free(ptr);

    void* ptr2 = dc_realloc(NULL, (size_t)16);
    TEST_ASSERT_NOT_NULL(ptr2, "hook realloc null uses alloc");
    dc_free(ptr2);

    TEST_ASSERT_EQ((size_t)2, hook_alloc_calls, "alloc hook count");
    TEST_ASSERT_EQ((size_t)1, hook_realloc_calls, "realloc hook count");
    TEST_ASSERT_EQ((size_t)2, hook_free_calls, "free hook count");

    dc_alloc_reset_hooks();
}

int test_alloc_main(void) {
    TEST_SUITE_BEGIN("Allocation Tests");
    
    test_basic_allocation();
    test_string_duplication();
    test_safe_free();
    test_realloc_behavior();
    test_alloc_hooks();
    
    TEST_SUITE_END("Allocation Tests");
}
