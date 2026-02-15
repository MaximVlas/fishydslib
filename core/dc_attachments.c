/**
 * @file dc_attachments.c
 * @brief Attachment helpers and validation
 */

#include "dc_attachments.h"
#include <string.h>

int dc_attachment_filename_is_valid(const char* filename) {
    if (!filename) return 0;
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) return 0;
    const unsigned char* p = (const unsigned char*)filename;
    if (*p == '\0') return 0;
    for (; *p; p++) {
        unsigned char c = *p;
        if (c & 0x80u) return 0;
        if (c >= (unsigned char)'0' && c <= (unsigned char)'9') continue;
        if (c >= (unsigned char)'A' && c <= (unsigned char)'Z') continue;
        if (c >= (unsigned char)'a' && c <= (unsigned char)'z') continue;
        if (c == (unsigned char)'_' || c == (unsigned char)'-' || c == (unsigned char)'.') continue;

        return 0;
    }

    return 1;
}

int dc_attachment_size_is_valid(size_t size, size_t max_size) {
    return (max_size == 0) ? 1 : (size <= max_size);
}

int dc_attachment_total_size_is_valid(size_t total, size_t max_total) {
    return (max_total == 0) ? 1 : (total <= max_total);
}
