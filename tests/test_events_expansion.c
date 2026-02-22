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

void test_parse_ready_with_extended_user_fields(void) {
    const char* json = "{"
        "\"v\": 10,"
        "\"user\": {"
            "\"id\": \"12345\","
            "\"username\": \"Bot\","
            "\"avatar_decoration_data\": {\"asset\":\"asset_hash\",\"sku_id\":\"555\"},"
            "\"collectibles\": {\"nameplate\": {\"sku_id\":\"777\",\"asset\":\"np_asset\",\"label\":\"VIP\",\"palette\":\"violet\"}},"
            "\"primary_guild\": {\"identity_guild_id\":\"888\",\"identity_enabled\":true,\"tag\":\"FISH\",\"badge\":\"badge_hash\"}"
        "},"
        "\"guilds\": [{\"id\": \"22222\", \"unavailable\": true}],"
        "\"session_id\": \"sess_456\","
        "\"resume_gateway_url\": \"wss://resume.discord.gg\""
    "}";

    dc_gateway_ready_t ready;
    TEST_ASSERT_EQ(DC_OK, dc_gateway_event_parse_ready(json, &ready), "parse ready with extended user");
    TEST_ASSERT_EQ(1, ready.user.has_avatar_decoration_data, "ready user avatar decoration data present");
    TEST_ASSERT_EQ(1, ready.user.has_collectibles, "ready user collectibles present");
    TEST_ASSERT_EQ(1, ready.user.collectibles.has_nameplate, "ready user nameplate present");
    TEST_ASSERT_EQ(1, ready.user.has_primary_guild, "ready user primary guild present");
    TEST_ASSERT_STR_EQ("FISH", dc_string_cstr(&ready.user.primary_guild.tag.value), "ready user primary guild tag");
    TEST_ASSERT_STR_EQ("VIP", dc_string_cstr(&ready.user.collectibles.nameplate.label), "ready user nameplate label");
    dc_gateway_ready_free(&ready);
}

void test_parse_message_with_documented_extended_fields(void) {
    const char* json = "{"
        "\"id\": \"44444\","
        "\"content\": \"Extended fields\","
        "\"author\": {\"id\": \"12345\", \"username\": \"User\", \"discriminator\": \"0000\"},"
        "\"channel_id\": \"11111\","
        "\"timestamp\": \"2024-01-01T00:00:00.000Z\","
        "\"application\": {\"id\":\"777\",\"name\":\"My App\"},"
        "\"message_snapshots\": [{\"message\": {\"type\":0,\"content\":\"forwarded\",\"embeds\":[],\"attachments\":[],\"timestamp\":\"2024-01-01T00:00:00.000Z\",\"edited_timestamp\":null,\"flags\":0,\"mentions\":[],\"mention_roles\":[],\"sticker_items\":[],\"components\":[]}}],"
        "\"interaction_metadata\": {\"id\":\"333\",\"type\":2,\"user\":{\"id\":\"12345\",\"username\":\"User\"},\"authorizing_integration_owners\":{\"0\":\"12345\"}},"
        "\"resolved\": {\"users\": {\"12345\": {\"id\":\"12345\", \"username\":\"User\"}}},"
        "\"poll\": {\"question\":{\"text\":\"Q\"},\"answers\":[{\"answer_id\":1,\"poll_media\":{\"text\":\"A\"}}],\"expiry\":\"2025-01-01T00:00:00.000Z\",\"allow_multiselect\":false,\"layout_type\":1},"
        "\"tts\": false,"
        "\"mention_everyone\": false,"
        "\"pinned\": false,"
        "\"type\": 0"
    "}";

    dc_message_t msg;
    TEST_ASSERT_EQ(DC_OK, dc_gateway_event_parse_message_create(json, &msg),
                   "parse message with documented extended fields");
    TEST_ASSERT_EQ(1, msg.has_application, "message has application");
    TEST_ASSERT_EQ(1, msg.has_message_snapshots, "message has message snapshots");
    TEST_ASSERT_EQ(1, msg.has_interaction_metadata, "message has interaction metadata");
    TEST_ASSERT_EQ(1, msg.has_resolved, "message has resolved");
    TEST_ASSERT_EQ(1, msg.has_poll, "message has poll");
    TEST_ASSERT_NEQ(NULL, strstr(dc_string_cstr(&msg.application_json), "\"id\":\"777\""),
                    "application json includes id");
    TEST_ASSERT_NEQ(NULL, strstr(dc_string_cstr(&msg.poll_json), "\"layout_type\":1"),
                    "poll json includes layout type");
    dc_message_free(&msg);
}

void test_gateway_event_kind_includes_interaction_create(void) {
    TEST_ASSERT_EQ(DC_GATEWAY_EVENT_INTERACTION_CREATE,
                   dc_gateway_event_kind_from_name("INTERACTION_CREATE"),
                   "event kind maps INTERACTION_CREATE");
}

void test_parse_interaction_create_application_command(void) {
    const char* json = "{"
        "\"id\":\"100000\","
        "\"application_id\":\"200000\","
        "\"type\":2,"
        "\"data\":{"
            "\"id\":\"300000\","
            "\"name\":\"ping\","
            "\"type\":1,"
            "\"options\":[{\"name\":\"target\",\"type\":3,\"value\":\"abc\"}],"
            "\"resolved\":{\"users\":{\"123\":{\"id\":\"123\",\"username\":\"User\"}}}"
        "},"
        "\"guild_id\":\"400000\","
        "\"channel_id\":\"500000\","
        "\"member\":{\"user\":{\"id\":\"123\",\"username\":\"User\",\"discriminator\":\"0000\"},\"roles\":[]},"
        "\"token\":\"tok_123\","
        "\"version\":1,"
        "\"app_permissions\":\"2147483648\","
        "\"locale\":\"en-US\","
        "\"guild_locale\":\"en-US\","
        "\"context\":0,"
        "\"entitlements\":[],"
        "\"authorizing_integration_owners\":{\"0\":\"400000\"}"
    "}";

    dc_interaction_t interaction;
    TEST_ASSERT_EQ(DC_OK, dc_gateway_event_parse_interaction_create(json, &interaction),
                   "parse application command interaction");
    TEST_ASSERT_EQ(100000ULL, interaction.id, "interaction id");
    TEST_ASSERT_EQ(DC_INTERACTION_TYPE_APPLICATION_COMMAND, interaction.type, "interaction type");
    TEST_ASSERT_EQ(1, interaction.guild_id.is_set, "interaction has guild id");
    TEST_ASSERT_EQ(400000ULL, interaction.guild_id.value, "interaction guild id");
    TEST_ASSERT_EQ(1, interaction.has_member, "interaction has member");
    TEST_ASSERT_EQ(1, interaction.has_data, "interaction has data");
    TEST_ASSERT_EQ(1, interaction.data.has_name, "interaction data has name");
    TEST_ASSERT_STR_EQ("ping", dc_string_cstr(&interaction.data.name), "interaction command name");
    TEST_ASSERT_EQ(1, interaction.data.has_options, "interaction data has options");
    TEST_ASSERT_EQ(1, interaction.data.has_resolved, "interaction data has resolved");
    TEST_ASSERT_NEQ(NULL, strstr(dc_string_cstr(&interaction.data.options_json), "\"target\""),
                    "options json includes option name");
    TEST_ASSERT_EQ(1, interaction.app_permissions.is_set, "interaction has app_permissions");
    TEST_ASSERT_EQ(1, interaction.has_authorizing_integration_owners,
                   "interaction has authorizing integration owners");
    dc_interaction_free(&interaction);
}

void test_parse_interaction_create_component_dm(void) {
    const char* json = "{"
        "\"id\":\"101\","
        "\"application_id\":\"202\","
        "\"type\":3,"
        "\"data\":{"
            "\"custom_id\":\"btn_ok\","
            "\"component_type\":2,"
            "\"values\":[\"x\"]"
        "},"
        "\"channel_id\":\"303\","
        "\"user\":{\"id\":\"404\",\"username\":\"DmUser\",\"discriminator\":\"0000\"},"
        "\"message\":{"
            "\"id\":\"505\","
            "\"content\":\"Click\","
            "\"author\":{\"id\":\"606\",\"username\":\"Bot\",\"discriminator\":\"0000\"},"
            "\"channel_id\":\"303\","
            "\"timestamp\":\"2024-01-01T00:00:00.000Z\","
            "\"tts\":false,"
            "\"mention_everyone\":false,"
            "\"pinned\":false,"
            "\"type\":0"
        "},"
        "\"token\":\"tok_component\","
        "\"version\":1,"
        "\"locale\":\"en-US\""
    "}";

    dc_interaction_t interaction;
    TEST_ASSERT_EQ(DC_OK, dc_gateway_event_parse_interaction_create(json, &interaction),
                   "parse component interaction");
    TEST_ASSERT_EQ(DC_INTERACTION_TYPE_MESSAGE_COMPONENT, interaction.type, "component type");
    TEST_ASSERT_EQ(1, interaction.has_user, "component has user");
    TEST_ASSERT_EQ(1, interaction.has_message, "component has message");
    TEST_ASSERT_EQ(1, interaction.has_data, "component has data");
    TEST_ASSERT_EQ(1, interaction.data.has_custom_id, "component has custom id");
    TEST_ASSERT_STR_EQ("btn_ok", dc_string_cstr(&interaction.data.custom_id), "component custom id");
    TEST_ASSERT_EQ(1, interaction.data.has_component_type, "component has component type");
    TEST_ASSERT_EQ(2, interaction.data.component_type, "component type value");
    TEST_ASSERT_EQ(1, interaction.data.has_values, "component has values");
    TEST_ASSERT_NEQ(NULL, strstr(dc_string_cstr(&interaction.data.values_json), "\"x\""),
                    "component values include selection");
    TEST_ASSERT_EQ(0, interaction.has_context, "component context absent");
    dc_interaction_free(&interaction);
}
