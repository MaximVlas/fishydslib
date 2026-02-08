/**
 * @file test_gateway_main.c
 * @brief Main entry point for Gateway tests
 */

/* Declare test functions */
void test_gateway_close_code_strings(void);
void test_gateway_close_code_reconnect(void);
void test_gateway_client_create_invalid(void);
void test_gateway_client_create_success(void);
void test_gateway_update_presence_invalid(void);
void test_gateway_client_connect_invalid(void);
void test_gateway_client_process_invalid(void);
void test_gateway_request_guild_members_invalid(void);
void test_gateway_request_soundboard_invalid(void);
void test_gateway_update_voice_state_invalid(void);

#include <stdio.h>
#include "test_utils.h"

int main(void) {
    printf("Running Gateway client tests...\n\n");

    test_gateway_close_code_strings();
    test_gateway_close_code_reconnect();
    test_gateway_client_create_invalid();
    test_gateway_client_create_success();
    test_gateway_update_presence_invalid();
    test_gateway_client_connect_invalid();
    test_gateway_client_process_invalid();
    test_gateway_request_guild_members_invalid();
    test_gateway_request_soundboard_invalid();
    test_gateway_update_voice_state_invalid();

    printf("\n=== Gateway Client Test Summary ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passed);
    printf("Failed: %d\n", test_failed);

    return (test_failed == 0) ? 0 : 1;
}
