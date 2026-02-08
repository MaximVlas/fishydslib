/**
 * @file test_core_main.c
 * @brief Core tests main function
 */

#include "test_utils.h"

/* Test function declarations */
int test_status_main(void);
int test_alloc_main(void);
int test_string_main(void);
int test_vec_main(void);
int test_snowflake_main(void);
int test_time_main(void);
int test_optional_main(void);
int test_format_main(void);
int test_allowed_mentions_main(void);
int test_cdn_main(void);
int test_data_uri_main(void);
int test_attachments_main(void);
int test_env_main(void);
int test_permissions_main(void);

int main(void) {
    int result = 0;
    
    printf("Running fishydslib core tests...\n");
    
    result |= test_status_main();
    result |= test_alloc_main();
    result |= test_string_main();
    result |= test_vec_main();
    result |= test_snowflake_main();
    result |= test_time_main();
    result |= test_optional_main();
    result |= test_format_main();
    result |= test_allowed_mentions_main();
    result |= test_cdn_main();
    result |= test_data_uri_main();
    result |= test_attachments_main();
    result |= test_env_main();
    result |= test_permissions_main();
    
    if (result == 0) {
        printf("\nAll core tests passed!\n");
    } else {
        printf("\nSome core tests failed!\n");
    }
    
    return result;
}
