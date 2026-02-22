#include <stdio.h>
#include "test_utils.h"

void test_parse_ready(void);
void test_parse_guild_create(void);

void test_parse_message_create(void);
void test_parse_message_create_full(void);
void test_parse_message_create_dm(void);
void test_parse_message_with_extra_fields(void);
void test_parse_ready_with_extended_user_fields(void);
void test_parse_message_with_documented_extended_fields(void);
void test_gateway_event_kind_includes_interaction_create(void);
void test_parse_interaction_create_application_command(void);
void test_parse_interaction_create_component_dm(void);

int main(void) {
    printf("Running Gateway Events Expansion tests...\n\n");

    test_parse_ready();
    test_parse_guild_create();
    test_parse_message_create();
    test_parse_message_create_full();
    test_parse_message_create_dm();
    test_parse_message_with_extra_fields();
    test_parse_ready_with_extended_user_fields();
    test_parse_message_with_documented_extended_fields();
    test_gateway_event_kind_includes_interaction_create();
    test_parse_interaction_create_application_command();
    test_parse_interaction_create_component_dm();

    printf("\n=== Gateway Events Expansion Test Summary ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passed);
    printf("Failed: %d\n", test_failed);

    return (test_failed == 0) ? 0 : 1;
}
