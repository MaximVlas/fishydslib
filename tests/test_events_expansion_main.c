#include <stdio.h>
#include "test_utils.h"

void test_parse_ready(void);
void test_parse_guild_create(void);

void test_parse_message_create(void);
void test_parse_message_create_full(void);
void test_parse_message_create_dm(void);
void test_parse_message_with_extra_fields(void);

int main(void) {
    printf("Running Gateway Events Expansion tests...\n\n");

    test_parse_ready();
    test_parse_guild_create();
    test_parse_message_create();
    test_parse_message_create_full();
    test_parse_message_create_dm();
    test_parse_message_with_extra_fields();

    printf("\n=== Gateway Events Expansion Test Summary ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passed);
    printf("Failed: %d\n", test_failed);

    return (test_failed == 0) ? 0 : 1;
}
