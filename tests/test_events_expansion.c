#include "test_utils.h"
#include "gw/dc_events.h"
#include "core/dc_status.h"
#include "core/dc_string.h"
#include <string.h>

void test_parse_ready(void) {
    const char* json = "{"
        "\"v\": 10,"
        "\"user\": {\"id\": \"12345\", \"username\": \"Bot\", \"discriminator\": \"1234\"},"
        "\"guilds\": [{\"id\": \"22222\", \"unavailable\": true}],"
        "\"session_id\": \"sess_123\","
        "\"resume_gateway_url\": \"wss://resume.discord.gg\","
        "\"shard\": [0, 1],"
        "\"application\": {\"id\": \"98765\", \"flags\": 1}"
    "}";
    
    dc_gateway_ready_t ready;
    TEST_ASSERT_EQ(DC_OK, dc_gateway_event_parse_ready(json, &ready), "parse ready ok");
    TEST_ASSERT_EQ(10, ready.v, "version 10");
    TEST_ASSERT_EQ(12345, ready.user.id, "user id");
    TEST_ASSERT_STR_EQ("sess_123", dc_string_cstr(&ready.session_id), "session id");
    
    TEST_ASSERT_EQ(1, ready.application_id.is_set, "app id set");
    TEST_ASSERT_EQ(98765, ready.application_id.value, "app id value");

    TEST_ASSERT_EQ(1u, dc_vec_length(&ready.guilds), "guilds count");
    const dc_gateway_unavailable_guild_t* g =
        (const dc_gateway_unavailable_guild_t*)dc_vec_at(&ready.guilds, (size_t)0);
    TEST_ASSERT_NOT_NULL(g, "guild entry not null");
    TEST_ASSERT_EQ(22222ULL, g->id, "guild id");
    TEST_ASSERT_EQ(1, g->unavailable, "guild unavailable");
    
    dc_gateway_ready_free(&ready);
}

void test_parse_guild_create(void) {
     const char* json = "{"
        "\"id\": \"1001\","
        "\"name\": \"Test Guild\","
        "\"joined_at\": \"2023-01-01T00:00:00+00:00\","
        "\"member_count\": 5,"
        "\"members\": [],"
        "\"channels\": [],"
        "\"threads\": [],"
        "\"voice_states\": ["
            "{\"guild_id\": \"1001\", \"channel_id\": \"2001\", \"user_id\": \"12345\", \"session_id\": \"sess1\", \"deaf\": false, \"mute\": true, \"self_deaf\": false, \"self_mute\": true}"
        "],"
        "\"presences\": ["
            "{\"user\": {\"id\": \"12345\"}, \"status\": \"dnd\"}"
        "]"
    "}";
    
    dc_gateway_guild_create_t guild;
    TEST_ASSERT_EQ(DC_OK, dc_gateway_event_parse_guild_create(json, &guild), "parse guild create ok");
    TEST_ASSERT_EQ(1001, guild.guild.id, "guild id");
    TEST_ASSERT_STR_EQ("Test Guild", dc_string_cstr(&guild.guild.name), "guild name");
    TEST_ASSERT_EQ(5, guild.member_count, "member count");
    
    // Verify voice states
    TEST_ASSERT_EQ(1, dc_vec_length(&guild.voice_states), "voice states count");
    dc_voice_state_t* vs = (dc_voice_state_t*)dc_vec_at(&guild.voice_states, 0);
    TEST_ASSERT_EQ(2001, vs->channel_id, "voice state channel id");
    TEST_ASSERT_EQ(12345, vs->user_id, "voice state user id");
    TEST_ASSERT_EQ(1, vs->mute, "voice state server mute");
    TEST_ASSERT_EQ(1, vs->self_mute, "voice state self mute");
    
    // Verify presences
    TEST_ASSERT_EQ(1, dc_vec_length(&guild.presences), "presences count");
    dc_presence_t* p = (dc_presence_t*)dc_vec_at(&guild.presences, 0);
    TEST_ASSERT_EQ(12345, p->user_id, "presence user id");
    TEST_ASSERT_EQ(DC_PRESENCE_STATUS_DND, p->status, "presence status enum");
    TEST_ASSERT_STR_EQ("dnd", dc_string_cstr(&p->status_str), "presence status string");

    dc_gateway_guild_create_free(&guild);
}

void test_parse_message_create(void) {
    const char* json = "{"
        "\"id\": \"99999\","
        "\"content\": \"Hello world\","
        "\"author\": {\"id\": \"12345\", \"username\": \"User\", \"discriminator\": \"0000\"},"
        "\"channel_id\": \"55555\","
        "\"timestamp\": \"2023-01-01T00:00:00+00:00\""
    "}";

    dc_message_t msg;
    TEST_ASSERT_EQ(DC_OK, dc_gateway_event_parse_message_create(json, &msg), "parse message create ok");
    TEST_ASSERT_EQ(99999, msg.id, "message id");
    TEST_ASSERT_STR_EQ("Hello world", dc_string_cstr(&msg.content), "message content");
    TEST_ASSERT_EQ(12345, msg.author.id, "author id");
    
    dc_message_free(&msg);
}

void test_parse_message_create_full(void) {
    const char* json = "{"
        "\"id\": \"88888\","
        "\"content\": \"Hello Guild\","
        "\"author\": {\"id\": \"11111\", \"username\": \"GuildMember\", \"discriminator\": \"0000\"},"
        "\"channel_id\": \"22222\","
        "\"timestamp\": \"2023-02-01T12:00:00+00:00\","
        "\"guild_id\": \"33333\","
        "\"member\": {\"nick\": \"NickName\", \"roles\": [\"44444\"]}"
    "}";

    dc_gateway_message_create_t msg;
    TEST_ASSERT_EQ(DC_OK, dc_gateway_event_parse_message_create_full(json, &msg), "parse full message ok");
    
    TEST_ASSERT_EQ(88888, msg.message.id, "message id");
    TEST_ASSERT_EQ(33333, msg.guild_id.value, "guild id");
    TEST_ASSERT_EQ(1, msg.has_member, "has member");
    TEST_ASSERT_STR_EQ("NickName", dc_string_cstr(&msg.member.nick.value), "member nick");
    
    dc_gateway_message_create_free(&msg);
}

void test_parse_message_create_dm(void) {
    const char* json = "{"
        "\"id\": \"77777\","
        "\"content\": \"Hello DM\","
        "\"author\": {\"id\": \"99999\", \"username\": \"DMUser\", \"discriminator\": \"0000\"},"
        "\"channel_id\": \"66666\","
        "\"timestamp\": \"2023-02-01T12:00:00+00:00\"" 
    "}";

    dc_gateway_message_create_t msg;
    TEST_ASSERT_EQ(DC_OK, dc_gateway_event_parse_message_create_full(json, &msg), "parse dm message ok");
    
    TEST_ASSERT_EQ(77777, msg.message.id, "message id");
    TEST_ASSERT_EQ(0, msg.guild_id.is_set, "guild id not set");
    TEST_ASSERT_EQ(0, msg.has_member, "no member");
    
    dc_gateway_message_create_free(&msg);
}

void test_parse_message_with_extra_fields(void) {
    const char* json = "{"
        "\"id\": \"55555\","
        "\"content\": \"Rich Message\","
        "\"author\": {\"id\": \"12345\", \"username\": \"User\", \"discriminator\": \"0000\"},"
        "\"channel_id\": \"11111\","
        "\"timestamp\": \"2023-03-01T00:00:00+00:00\","
	        "\"attachments\": [{"
	            "\"id\": \"101010\","
	            "\"filename\": \"test.png\","
	            "\"size\": 1024,"
	            "\"url\": \"https://example.com/test.png\","
	            "\"proxy_url\": \"https://example.com/test.png\""
	        "}],"
	        "\"embeds\": [{"
	            "\"title\": \"Embed Title\","
	            "\"description\": \"Embed Desc\","
            "\"color\": 16711680"
        "}],"
        "\"mentions\": [{"
             "\"id\": \"998877\","
             "\"username\": \"MentionedUser\","
             "\"member\": {\"nick\": \"MentionedNick\"}" 
        "}]"
    "}";

    dc_message_t msg;
    TEST_ASSERT_EQ(DC_OK, dc_gateway_event_parse_message_create(json, &msg), "parse rich message ok");
    
    // Check attachments
    TEST_ASSERT_EQ(1, dc_vec_length(&msg.attachments), "attachments count");
    dc_attachment_t* att = (dc_attachment_t*)dc_vec_at(&msg.attachments, 0);
    TEST_ASSERT_EQ(101010, att->id, "attachment id");
    TEST_ASSERT_STR_EQ("test.png", dc_string_cstr(&att->filename), "attachment filename");
    TEST_ASSERT_EQ(1024, att->size, "attachment size");

    // Check embeds
    TEST_ASSERT_EQ(1, dc_vec_length(&msg.embeds), "embeds count");
    dc_embed_t* emb = (dc_embed_t*)dc_vec_at(&msg.embeds, 0);
    TEST_ASSERT_STR_EQ("Embed Title", dc_string_cstr(&emb->title.value), "embed title");
    TEST_ASSERT_EQ(16711680, emb->color, "embed color");

    // Check mentions
    TEST_ASSERT_EQ(1, dc_vec_length(&msg.mentions), "mentions count");
    dc_guild_member_t* ment = (dc_guild_member_t*)dc_vec_at(&msg.mentions, 0);
    TEST_ASSERT_EQ(998877, ment->user.id, "mentioned user id");
    TEST_ASSERT_STR_EQ("MentionedNick", dc_string_cstr(&ment->nick.value), "mentioned member nick");

    dc_message_free(&msg);
}
