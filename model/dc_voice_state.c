#include "model/dc_voice_state.h"
#include <string.h>

dc_status_t dc_voice_state_init(dc_voice_state_t* vs) {
    if (!vs) return DC_ERROR_NULL_POINTER;
    memset(vs, 0, sizeof(*vs));
    dc_status_t st = dc_string_init(&vs->session_id);
    if (st != DC_OK) return st;
    st = dc_string_init(&vs->request_to_speak_timestamp.value);
    if (st != DC_OK) {
        dc_string_free(&vs->session_id);
        return st;
    }
    vs->request_to_speak_timestamp.is_null = 1;
    return DC_OK;
}

void dc_voice_state_free(dc_voice_state_t* vs) {
    if (!vs) return;
    dc_string_free(&vs->session_id);
    dc_string_free(&vs->request_to_speak_timestamp.value);
    memset(vs, 0, sizeof(*vs));
}
