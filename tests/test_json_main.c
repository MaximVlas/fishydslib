/**
 * @file test_json_main.c
 * @brief JSON tests main function
 */

#include "test_utils.h"

/* Test function declarations */
int test_json_main(void);

int main(void) {
    int result = 0;
    
    printf("Running fishydslib JSON tests...\n");
    
    result |= test_json_main();
    
    if (result == 0) {
        printf("\nAll JSON tests passed!\n");
    } else {
        printf("\nSome JSON tests failed!\n");
    }
    
    return result;
}