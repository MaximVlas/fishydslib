/**
 * @file dc_attachments.c
 * @brief Attachment helpers and validation
 */

#include "dc_attachments.h"

static int dc_attachment_is_ascii(unsigned char c) {
    return c <= 0x7E;
}

int dc_attachment_filename_is_valid(const char* filename) {
    if (!filename || filename[0] == '\0') return 0;
    for (const unsigned char* p = (const unsigned char*)filename; *p; p++) {
        unsigned char c = *p;
        if (!dc_attachment_is_ascii(c)) return 0;
        if (c < 0x20) return 0;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            continue;
        }
        if (c == '_' || c == '-' || c == '.') {
            continue;
        }
        return 0;
    }
    return 1;
}

int dc_attachment_size_is_valid(size_t size, size_t max_size) {
    if (max_size == 0) return 1;
    return size <= max_size;
}

int dc_attachment_total_size_is_valid(size_t total, size_t max_total) {
    if (max_total == 0) return 1;
    return total <= max_total;
}
