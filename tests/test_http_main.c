/**
 * @file test_http_main.c
 * @brief HTTP tests main function
 */

#include "test_utils.h"

/* Test function declarations */
int test_http_main(void);
int test_multipart_main(void);

int main(void) {
    int result = 0;
    
    printf("Running fishydslib HTTP tests...\n");
    
    result |= test_http_main();
    result |= test_multipart_main();
    
    if (result == 0) {
        printf("\nAll HTTP tests passed!\n");
    } else {
        printf("\nSome HTTP tests failed!\n");
    }
    
    return result;
}
