/**
 * @file test_rest_main.c
 * @brief Main entry point for REST client tests
 */

/* Declare test functions */
void test_rest_client_create(void);
void test_rest_client_create_invalid(void);
void test_rest_request_init(void);
void test_rest_request_set_path(void);
void test_rest_request_set_json_body(void);
void test_rest_request_headers(void);
void test_rest_execute_basic(void);
void test_rest_execute_with_rate_limit_headers(void);
void test_rest_execute_429_retry(void);
void test_rest_execute_error_parsing(void);
void test_rest_interaction_endpoint_exemption(void);
void test_rest_auth_header_injection(void);

#include <stdio.h>
#include "test_utils.h"

int main(void) {
    printf("Running REST client tests...\n\n");
    
    test_rest_client_create();
    test_rest_client_create_invalid();
    test_rest_request_init();
    test_rest_request_set_path();
    test_rest_request_set_json_body();
    test_rest_request_headers();
    test_rest_execute_basic();
    test_rest_execute_with_rate_limit_headers();
    test_rest_execute_429_retry();
    test_rest_execute_error_parsing();
    test_rest_interaction_endpoint_exemption();
    test_rest_auth_header_injection();
    
    printf("\n=== REST Client Test Summary ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passed);
    printf("Failed: %d\n", test_failed);
    
    return (test_failed == 0) ? 0 : 1;
}
