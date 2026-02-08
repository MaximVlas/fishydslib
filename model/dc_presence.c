#include "model/dc_presence.h"
#include <string.h>

dc_status_t dc_presence_init(dc_presence_t* presence) {
    if (!presence) return DC_ERROR_NULL_POINTER;
    memset(presence, 0, sizeof(*presence));
    dc_status_t st = dc_string_init(&presence->status_str);
    if (st != DC_OK) return st;
    presence->status = DC_PRESENCE_STATUS_OFFLINE;
    return DC_OK;
}

void dc_presence_free(dc_presence_t* presence) {
    if (!presence) return;
    dc_string_free(&presence->status_str);
    memset(presence, 0, sizeof(*presence));
}

dc_presence_status_t dc_presence_status_from_string(const char* status_str) {
    if (!status_str) return DC_PRESENCE_STATUS_OFFLINE;
    if (strcmp(status_str, "online") == 0) return DC_PRESENCE_STATUS_ONLINE;
    if (strcmp(status_str, "idle") == 0) return DC_PRESENCE_STATUS_IDLE;
    if (strcmp(status_str, "dnd") == 0) return DC_PRESENCE_STATUS_DND;
    return DC_PRESENCE_STATUS_OFFLINE;
}
