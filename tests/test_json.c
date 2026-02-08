/**
 * @file test_json.c
 * @brief JSON tests
 */

#include "test_utils.h"
#include "json/dc_json.h"
#include "core/dc_string.h"
#include "model/dc_user.h"
#include "model/dc_guild.h"
#include "model/dc_guild_member.h"
#include "model/dc_role.h"
#include "model/dc_channel.h"
#include "model/dc_message.h"
#include <yyjson.h>

int test_json_main(void) {
    TEST_SUITE_BEGIN("JSON Tests");
    
    /* Parse simple JSON */
    const char* simple_json = "{\"name\":\"test\",\"value\":42}";
    dc_json_doc_t doc;
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(simple_json, &doc), "parse simple json");
    TEST_ASSERT_NEQ(NULL, doc.doc, "doc not null");
    TEST_ASSERT_NEQ(NULL, doc.root, "root not null");
    
    /* Get string field */
    const char* name = NULL;
    TEST_ASSERT_EQ(DC_OK, dc_json_get_string(doc.root, "name", &name), "get string");
    TEST_ASSERT_STR_EQ("test", name, "string value");
    
    /* Get int field */
    int64_t value = 0;
    TEST_ASSERT_EQ(DC_OK, dc_json_get_int64(doc.root, "value", &value), "get int64");
    TEST_ASSERT_EQ(42, value, "int64 value");
    
    /* Missing field */
    const char* missing = NULL;
    TEST_ASSERT_EQ(DC_ERROR_NOT_FOUND, dc_json_get_string(doc.root, "missing", &missing), "missing field");
    
    dc_json_doc_free(&doc);
    
    /* Parse JSON with optional fields */
    const char* opt_json = "{\"required\":\"yes\",\"optional\":null}";
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(opt_json, &doc), "parse optional json");
    
    const char* required = NULL;
    TEST_ASSERT_EQ(DC_OK, dc_json_get_string(doc.root, "required", &required), "get required");
    TEST_ASSERT_STR_EQ("yes", required, "required value");
    
    const char* optional = "default";
    TEST_ASSERT_EQ(DC_OK, dc_json_get_string_opt(doc.root, "optional", &optional, "default"), "get optional null");
    TEST_ASSERT_STR_EQ("default", optional, "optional default");
    
    const char* missing_opt = "default";
    TEST_ASSERT_EQ(DC_OK, dc_json_get_string_opt(doc.root, "missing", &missing_opt, "default"), "get missing optional");
    TEST_ASSERT_STR_EQ("default", missing_opt, "missing optional default");

    /* Optional/nullable helpers */
    dc_optional_cstr_t opt_name = {0};
    TEST_ASSERT_EQ(DC_OK, dc_json_get_string_optional(doc.root, "required", &opt_name), "optional present");
    TEST_ASSERT_EQ(1, opt_name.is_set, "optional is_set");
    TEST_ASSERT_STR_EQ("yes", opt_name.value, "optional value");

    dc_optional_cstr_t opt_missing2 = {0};
    TEST_ASSERT_EQ(DC_OK, dc_json_get_string_optional(doc.root, "missing2", &opt_missing2), "optional missing");
    TEST_ASSERT_EQ(0, opt_missing2.is_set, "optional missing is_set");
    TEST_ASSERT_EQ(NULL, opt_missing2.value, "optional missing value");

    dc_nullable_cstr_t nul_opt = {0};
    TEST_ASSERT_EQ(DC_OK, dc_json_get_string_nullable(doc.root, "optional", &nul_opt), "nullable null");
    TEST_ASSERT_EQ(1, nul_opt.is_null, "nullable is_null");
    TEST_ASSERT_EQ(NULL, nul_opt.value, "nullable value null");
    TEST_ASSERT_EQ(DC_ERROR_NOT_FOUND, dc_json_get_string_nullable(doc.root, "missing2", &nul_opt), "nullable missing");
    
    dc_json_doc_free(&doc);
    
    /* Parse JSON with bool */
    const char* bool_json = "{\"flag\":true}";
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(bool_json, &doc), "parse bool json");
    
    int flag = 0;
    TEST_ASSERT_EQ(DC_OK, dc_json_get_bool(doc.root, "flag", &flag), "get bool");
    TEST_ASSERT_EQ(1, flag, "bool value");
    
    dc_json_doc_free(&doc);
    
    /* Create mutable document */
    dc_json_mut_doc_t mut_doc;
    TEST_ASSERT_EQ(DC_OK, dc_json_mut_doc_create(&mut_doc), "create mut doc");
    TEST_ASSERT_NEQ(NULL, mut_doc.doc, "mut doc not null");
    TEST_ASSERT_NEQ(NULL, mut_doc.root, "mut root not null");
    
    /* Add fields */
    TEST_ASSERT_EQ(DC_OK, dc_json_mut_set_string(&mut_doc, mut_doc.root, "name", "test"), "set string");
    TEST_ASSERT_EQ(DC_OK, dc_json_mut_set_int64(&mut_doc, mut_doc.root, "value", (int64_t)42), "set int64");
    TEST_ASSERT_EQ(DC_OK, dc_json_mut_set_bool(&mut_doc, mut_doc.root, "flag", 1), "set bool");
    TEST_ASSERT_EQ(DC_OK, dc_json_mut_set_null(&mut_doc, mut_doc.root, "optional"), "set null");
    
    /* Serialize */
    dc_string_t result;
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&result), "init result string");
    TEST_ASSERT_EQ(DC_OK, dc_json_mut_doc_serialize(&mut_doc, &result), "serialize");
    TEST_ASSERT_NEQ(0u, dc_string_length(&result), "serialized not empty");
    
    dc_string_free(&result);
    dc_json_mut_doc_free(&mut_doc);

    /* Allowed mentions builder */
    dc_allowed_mentions_t mentions;
    TEST_ASSERT_EQ(DC_OK, dc_allowed_mentions_init(&mentions), "allowed mentions init");
    dc_allowed_mentions_set_parse(&mentions, 1, 0, 1);
    dc_allowed_mentions_set_replied_user(&mentions, 1);
    TEST_ASSERT_EQ(DC_OK, dc_allowed_mentions_add_user(&mentions, 123), "allowed mentions add user");
    TEST_ASSERT_EQ(DC_OK, dc_allowed_mentions_add_role(&mentions, 456), "allowed mentions add role");

    TEST_ASSERT_EQ(DC_OK, dc_json_mut_doc_create(&mut_doc), "create mut doc for allowed mentions");
    TEST_ASSERT_EQ(DC_OK,
                   dc_json_mut_add_allowed_mentions(&mut_doc, mut_doc.root, "allowed_mentions", &mentions),
                   "set allowed mentions");
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&result), "init result for allowed mentions");
    TEST_ASSERT_EQ(DC_OK, dc_json_mut_doc_serialize(&mut_doc, &result), "serialize allowed mentions");

    TEST_ASSERT_EQ(DC_OK, dc_json_parse(dc_string_cstr(&result), &doc), "parse allowed mentions");
    yyjson_val* am = yyjson_obj_get(doc.root, "allowed_mentions");
    TEST_ASSERT_NEQ(NULL, am, "allowed mentions object");
    yyjson_val* parse = yyjson_obj_get(am, "parse");
    TEST_ASSERT_NEQ(NULL, parse, "allowed mentions parse");
    TEST_ASSERT_EQ(2u, yyjson_arr_size(parse), "allowed mentions parse size");
    TEST_ASSERT_STR_EQ("users", yyjson_get_str(yyjson_arr_get(parse, 0)), "allowed mentions parse users");
    TEST_ASSERT_STR_EQ("everyone", yyjson_get_str(yyjson_arr_get(parse, 1)), "allowed mentions parse everyone");
    yyjson_val* users = yyjson_obj_get(am, "users");
    TEST_ASSERT_NEQ(NULL, users, "allowed mentions users");
    TEST_ASSERT_STR_EQ("123", yyjson_get_str(yyjson_arr_get(users, 0)), "allowed mentions users value");
    yyjson_val* roles = yyjson_obj_get(am, "roles");
    TEST_ASSERT_NEQ(NULL, roles, "allowed mentions roles");
    TEST_ASSERT_STR_EQ("456", yyjson_get_str(yyjson_arr_get(roles, 0)), "allowed mentions roles value");
    yyjson_val* replied = yyjson_obj_get(am, "replied_user");
    TEST_ASSERT_NEQ(NULL, replied, "allowed mentions replied user");
    TEST_ASSERT_EQ(1, yyjson_get_bool(replied), "allowed mentions replied user value");

    dc_json_doc_free(&doc);
    dc_string_free(&result);
    dc_json_mut_doc_free(&mut_doc);
    dc_allowed_mentions_free(&mentions);

    /* Attachments builder */
    dc_attachment_descriptor_t attachments[2];
    attachments[0].id = 0;
    attachments[0].filename = "file.png";
    attachments[0].description = "desc";
    attachments[1].id = 999;
    attachments[1].filename = NULL;
    attachments[1].description = NULL;

    TEST_ASSERT_EQ(DC_OK, dc_json_mut_doc_create(&mut_doc), "create mut doc for attachments");
    TEST_ASSERT_EQ(DC_OK,
                   dc_json_mut_add_attachments(&mut_doc, mut_doc.root, "attachments", attachments, 2),
                   "set attachments");
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&result), "init result for attachments");
    TEST_ASSERT_EQ(DC_OK, dc_json_mut_doc_serialize(&mut_doc, &result), "serialize attachments");
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(dc_string_cstr(&result), &doc), "parse attachments");
    yyjson_val* att_arr = yyjson_obj_get(doc.root, "attachments");
    TEST_ASSERT_NEQ(NULL, att_arr, "attachments array present");
    TEST_ASSERT_EQ(2u, yyjson_arr_size(att_arr), "attachments array size");
    yyjson_val* att0 = yyjson_arr_get(att_arr, 0);
    yyjson_val* att1 = yyjson_arr_get(att_arr, 1);
    TEST_ASSERT_STR_EQ("file.png", yyjson_get_str(yyjson_obj_get(att0, "filename")),
                       "attachments filename");
    TEST_ASSERT_STR_EQ("desc", yyjson_get_str(yyjson_obj_get(att0, "description")),
                       "attachments description");
    TEST_ASSERT_EQ(999u, (unsigned)yyjson_get_uint(yyjson_obj_get(att1, "id")), "attachments id");

    dc_json_doc_free(&doc);
    dc_string_free(&result);
    dc_json_mut_doc_free(&mut_doc);

    /* Allowed mentions empty (should not add key) */
    TEST_ASSERT_EQ(DC_OK, dc_allowed_mentions_init(&mentions), "allowed mentions init empty");
    TEST_ASSERT_EQ(DC_OK, dc_json_mut_doc_create(&mut_doc), "create mut doc for allowed mentions empty");
    TEST_ASSERT_EQ(DC_OK,
                   dc_json_mut_add_allowed_mentions(&mut_doc, mut_doc.root, "allowed_mentions", &mentions),
                   "set allowed mentions empty");
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&result), "init result for allowed mentions empty");
    TEST_ASSERT_EQ(DC_OK, dc_json_mut_doc_serialize(&mut_doc, &result), "serialize allowed mentions empty");
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(dc_string_cstr(&result), &doc), "parse allowed mentions empty");
    TEST_ASSERT_EQ(NULL, yyjson_obj_get(doc.root, "allowed_mentions"), "allowed mentions empty missing");

    dc_json_doc_free(&doc);
    dc_string_free(&result);
    dc_json_mut_doc_free(&mut_doc);
    dc_allowed_mentions_free(&mentions);
    
    /* Parse invalid JSON */
    const char* invalid_json = "{invalid}";
    TEST_ASSERT_NEQ(DC_OK, dc_json_parse(invalid_json, &doc), "parse invalid json");
    
    /* Type mismatch */
    const char* type_json = "{\"num\":42}";
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(type_json, &doc), "parse type json");
    const char* num_str = NULL;
    TEST_ASSERT_NEQ(DC_OK, dc_json_get_string(doc.root, "num", &num_str), "type mismatch");
    dc_json_doc_free(&doc);
    
    /* Snowflake parsing */
    const char* snowflake_json = "{\"id\":\"123456789012345678\"}";
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(snowflake_json, &doc), "parse snowflake json");
    
    uint64_t id = 0;
    TEST_ASSERT_EQ(DC_OK, dc_json_get_snowflake(doc.root, "id", &id), "get snowflake");
    TEST_ASSERT_EQ(123456789012345678ULL, id, "snowflake value");
    
    dc_json_doc_free(&doc);
    
    /* Snowflake optional */
    const char* snowflake_opt_json = "{\"id\":null}";
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(snowflake_opt_json, &doc), "parse snowflake opt json");
    
    uint64_t opt_id = 999;
    TEST_ASSERT_EQ(DC_OK, dc_json_get_snowflake_opt(doc.root, "id", &opt_id, (uint64_t)999), "get snowflake opt null");
    TEST_ASSERT_EQ(999ULL, opt_id, "snowflake opt default");
    
    dc_json_doc_free(&doc);
    
    /* Snowflake mutable */
    TEST_ASSERT_EQ(DC_OK, dc_json_mut_doc_create(&mut_doc), "create mut doc for snowflake");
    TEST_ASSERT_EQ(DC_OK, dc_json_mut_set_snowflake(&mut_doc, mut_doc.root, "id", 123456789012345678ULL), "set snowflake");
    
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&result), "init result for snowflake");
    TEST_ASSERT_EQ(DC_OK, dc_json_mut_doc_serialize(&mut_doc, &result), "serialize snowflake");
    
    /* Parse back and verify */
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(dc_string_cstr(&result), &doc), "parse serialized snowflake");
    uint64_t parsed_id = 0;
    TEST_ASSERT_EQ(DC_OK, dc_json_get_snowflake(doc.root, "id", &parsed_id), "get parsed snowflake");
    TEST_ASSERT_EQ(123456789012345678ULL, parsed_id, "parsed snowflake value");
    
    dc_json_doc_free(&doc);
    dc_string_free(&result);
    dc_json_mut_doc_free(&mut_doc);

    /* Permission helpers */
    const char* perm_json = "{\"perm\":\"2048\"}";
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(perm_json, &doc), "parse perm json");
    uint64_t perm = 0;
    TEST_ASSERT_EQ(DC_OK, dc_json_get_permission(doc.root, "perm", &perm), "get permission");
    TEST_ASSERT_EQ(2048ULL, perm, "permission value");
    uint64_t perm_def = 99;
    TEST_ASSERT_EQ(DC_OK, dc_json_get_permission_opt(doc.root, "missing", &perm_def, 99ULL), "get permission default");
    TEST_ASSERT_EQ(99ULL, perm_def, "permission default");
    dc_json_doc_free(&doc);

    const char* perm_bad = "{\"perm\":2048}";
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(perm_bad, &doc), "parse perm bad json");
    TEST_ASSERT_NEQ(DC_OK, dc_json_get_permission(doc.root, "perm", &perm), "permission type mismatch");
    dc_json_doc_free(&doc);

    TEST_ASSERT_EQ(DC_OK, dc_json_mut_doc_create(&mut_doc), "create mut doc for permission");
    TEST_ASSERT_EQ(DC_OK, dc_json_mut_set_permission(&mut_doc, mut_doc.root, "perm", 4096ULL), "set permission");
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&result), "init result for permission");
    TEST_ASSERT_EQ(DC_OK, dc_json_mut_doc_serialize(&mut_doc, &result), "serialize permission");
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(dc_string_cstr(&result), &doc), "parse serialized permission");
    TEST_ASSERT_EQ(DC_OK, dc_json_get_permission(doc.root, "perm", &perm), "get parsed permission");
    TEST_ASSERT_EQ(4096ULL, perm, "parsed permission value");
    dc_json_doc_free(&doc);
    dc_string_free(&result);
    dc_json_mut_doc_free(&mut_doc);

    /* Model parsing: User */
    const char* user_json = "{\"id\":\"123\",\"username\":\"alice\"}";
    dc_user_t user;
    TEST_ASSERT_EQ(DC_OK, dc_user_init(&user), "init user");
    TEST_ASSERT_EQ(DC_OK, dc_user_from_json(user_json, &user), "parse user json");
    TEST_ASSERT_EQ(123ULL, user.id, "user id parsed");
    TEST_ASSERT_STR_EQ("alice", dc_string_cstr(&user.username), "user username parsed");
    dc_user_free(&user);

    const char* user_missing_id = "{\"username\":\"alice\"}";
    TEST_ASSERT_EQ(DC_OK, dc_user_init(&user), "init user missing id");
    TEST_ASSERT_EQ(DC_ERROR_NOT_FOUND, dc_user_from_json(user_missing_id, &user), "user missing id");
    dc_user_free(&user);

    TEST_ASSERT_EQ(DC_OK, dc_user_init(&user), "init user invalid json");
    TEST_ASSERT_NEQ(DC_OK, dc_user_from_json("{bad json}", &user), "user invalid json");
    dc_user_free(&user);

    /* Model parsing: Guild */
    const char* guild_json =
        "{\"id\":\"42\",\"name\":\"Guild Test\",\"owner_id\":\"7\",\"permissions\":\"8\","
        "\"preferred_locale\":\"en-US\",\"premium_tier\":2,\"premium_progress_bar_enabled\":true,"
        "\"icon\":null,\"description\":\"Testing guild model\",\"approximate_member_count\":123}";
    dc_guild_t guild;
    TEST_ASSERT_EQ(DC_OK, dc_guild_init(&guild), "init guild");
    TEST_ASSERT_EQ(DC_OK, dc_guild_from_json(guild_json, &guild), "parse guild json");
    TEST_ASSERT_EQ(42ULL, guild.id, "guild id parsed");
    TEST_ASSERT_STR_EQ("Guild Test", dc_string_cstr(&guild.name), "guild name parsed");
    TEST_ASSERT_EQ(1, guild.owner_id.is_set, "guild owner_id is_set");
    TEST_ASSERT_EQ(7ULL, guild.owner_id.value, "guild owner_id value");
    TEST_ASSERT_EQ(1, guild.permissions.is_set, "guild permissions is_set");
    TEST_ASSERT_EQ(8ULL, guild.permissions.value, "guild permissions value");
    TEST_ASSERT_EQ(1, guild.icon.is_null, "guild icon null");
    TEST_ASSERT_EQ(0, guild.description.is_null, "guild description present");
    TEST_ASSERT_STR_EQ("Testing guild model", dc_string_cstr(&guild.description.value),
                       "guild description value");
    TEST_ASSERT_EQ(1, guild.approximate_member_count.is_set, "guild approximate member count set");
    TEST_ASSERT_EQ(123, guild.approximate_member_count.value, "guild approximate member count value");

    TEST_ASSERT_EQ(DC_OK, dc_string_init(&result), "init guild to_json result");
    TEST_ASSERT_EQ(DC_OK, dc_guild_to_json(&guild, &result), "guild to json");
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(dc_string_cstr(&result), &doc), "parse serialized guild");
    {
        uint64_t guild_id = 0;
        TEST_ASSERT_EQ(DC_OK, dc_json_get_snowflake(doc.root, "id", &guild_id),
                       "serialized guild id");
        TEST_ASSERT_EQ(42ULL, guild_id, "serialized guild id value");
    }
    dc_json_doc_free(&doc);
    dc_string_free(&result);
    dc_guild_free(&guild);

    const char* guild_missing_id = "{\"name\":\"Guild Test\"}";
    TEST_ASSERT_EQ(DC_OK, dc_guild_init(&guild), "init guild missing id");
    TEST_ASSERT_EQ(DC_ERROR_NOT_FOUND, dc_guild_from_json(guild_missing_id, &guild),
                   "guild missing id");
    dc_guild_free(&guild);

    /* Model parsing: Role */
    const char* role_json =
        "{\"id\":\"11\",\"name\":\"Mod\",\"color\":3447003,\"hoist\":true,\"icon\":null,"
        "\"unicode_emoji\":null,\"position\":2,\"permissions\":\"12345\",\"managed\":false,"
        "\"mentionable\":true,\"flags\":1,\"tags\":{\"bot_id\":\"222\",\"premium_subscriber\":null}}";
    dc_role_t role;
    TEST_ASSERT_EQ(DC_OK, dc_role_init(&role), "init role");
    TEST_ASSERT_EQ(DC_OK, dc_role_from_json(role_json, &role), "parse role json");
    TEST_ASSERT_EQ(11ULL, role.id, "role id parsed");
    TEST_ASSERT_STR_EQ("Mod", dc_string_cstr(&role.name), "role name parsed");
    TEST_ASSERT_EQ(3447003u, role.color, "role color parsed");
    TEST_ASSERT_EQ(1, role.hoist, "role hoist parsed");
    TEST_ASSERT_EQ(1, role.icon.is_null, "role icon null parsed");
    TEST_ASSERT_EQ(1, role.tags.bot_id.is_set, "role tags bot_id set");
    TEST_ASSERT_EQ(222ULL, role.tags.bot_id.value, "role tags bot_id value");
    TEST_ASSERT_EQ(1, role.tags.premium_subscriber.is_set, "role tags premium_subscriber set");
    TEST_ASSERT_EQ(1, role.tags.premium_subscriber.value, "role tags premium_subscriber value");

    TEST_ASSERT_EQ(DC_OK, dc_string_init(&result), "init role to_json result");
    TEST_ASSERT_EQ(DC_OK, dc_role_to_json(&role, &result), "role to json");
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(dc_string_cstr(&result), &doc), "parse serialized role");
    {
        uint64_t role_id = 0;
        TEST_ASSERT_EQ(DC_OK, dc_json_get_snowflake(doc.root, "id", &role_id),
                       "serialized role id");
        TEST_ASSERT_EQ(11ULL, role_id, "serialized role id value");
    }
    dc_json_doc_free(&doc);
    dc_string_free(&result);
    dc_role_free(&role);

    /* Model parsing: Guild member */
    const char* guild_member_json =
        "{\"user\":{\"id\":\"123\",\"username\":\"alice\"},\"nick\":\"Ali\",\"avatar\":null,"
        "\"roles\":[\"11\",\"22\"],\"joined_at\":\"2024-01-01T00:00:00.000Z\","
        "\"premium_since\":null,\"deaf\":false,\"mute\":true,\"pending\":false,"
        "\"permissions\":\"8\",\"communication_disabled_until\":null,\"flags\":2}";
    dc_guild_member_t member;
    TEST_ASSERT_EQ(DC_OK, dc_guild_member_init(&member), "init guild member");
    TEST_ASSERT_EQ(DC_OK, dc_guild_member_from_json(guild_member_json, &member),
                   "parse guild member json");
    TEST_ASSERT_EQ(1, member.has_user, "guild member user set");
    TEST_ASSERT_EQ(123ULL, member.user.id, "guild member user id parsed");
    TEST_ASSERT_EQ(0, member.nick.is_null, "guild member nick present");
    TEST_ASSERT_STR_EQ("Ali", dc_string_cstr(&member.nick.value), "guild member nick value");
    TEST_ASSERT_EQ(1, member.avatar.is_null, "guild member avatar null");
    TEST_ASSERT_EQ(2u, dc_vec_length(&member.roles), "guild member roles count");
    TEST_ASSERT_EQ(22ULL, *(dc_snowflake_t*)dc_vec_at(&member.roles, 1),
                   "guild member roles second value");
    TEST_ASSERT_EQ(0, member.deaf, "guild member deaf parsed");
    TEST_ASSERT_EQ(1, member.mute, "guild member mute parsed");
    TEST_ASSERT_EQ(1, member.pending.is_set, "guild member pending set");
    TEST_ASSERT_EQ(0, member.pending.value, "guild member pending value");
    TEST_ASSERT_EQ(1, member.permissions.is_set, "guild member permissions set");
    TEST_ASSERT_EQ(8ULL, member.permissions.value, "guild member permissions value");

    TEST_ASSERT_EQ(DC_OK, dc_string_init(&result), "init guild member to_json result");
    TEST_ASSERT_EQ(DC_OK, dc_guild_member_to_json(&member, &result), "guild member to json");
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(dc_string_cstr(&result), &doc),
                   "parse serialized guild member");
    {
        yyjson_val* roles = yyjson_obj_get(doc.root, "roles");
        TEST_ASSERT_NEQ(NULL, roles, "serialized guild member roles");
        TEST_ASSERT_EQ(2u, yyjson_arr_size(roles), "serialized guild member roles size");
    }
    dc_json_doc_free(&doc);
    dc_string_free(&result);
    dc_guild_member_free(&member);

    /* Model parsing: Channel */
    const char* channel_json = "{\"id\":\"555\",\"type\":0,\"name\":\"general\"}";
    dc_channel_t channel;
    TEST_ASSERT_EQ(DC_OK, dc_channel_init(&channel), "init channel");
    TEST_ASSERT_EQ(DC_OK, dc_channel_from_json(channel_json, &channel), "parse channel json");
    TEST_ASSERT_EQ(555ULL, channel.id, "channel id parsed");
    TEST_ASSERT_EQ(DC_CHANNEL_TYPE_GUILD_TEXT, channel.type, "channel type parsed");
    TEST_ASSERT_STR_EQ("general", dc_string_cstr(&channel.name), "channel name parsed");
    dc_channel_free(&channel);

    const char* channel_overwrites_json =
        "{\"id\":\"556\",\"type\":0,\"name\":\"general\","
        "\"permission_overwrites\":["
        "{\"id\":\"100\",\"type\":0,\"allow\":\"8\",\"deny\":\"0\"},"
        "{\"id\":\"500\",\"type\":1,\"allow\":null,\"deny\":\"4\"}"
        "]}";
    TEST_ASSERT_EQ(DC_OK, dc_channel_init(&channel), "init channel overwrites");
    TEST_ASSERT_EQ(DC_OK, dc_channel_from_json(channel_overwrites_json, &channel),
                   "parse channel overwrites json");
    TEST_ASSERT_EQ(2u, dc_vec_length(&channel.permission_overwrites), "channel overwrites count");
    {
        const dc_permission_overwrite_t* ow0 = dc_vec_at(&channel.permission_overwrites, (size_t)0);
        TEST_ASSERT_NEQ(NULL, ow0, "overwrite[0] not null");
        TEST_ASSERT_EQ(100ULL, ow0->id, "overwrite[0] id");
        TEST_ASSERT_EQ(DC_PERMISSION_OVERWRITE_TYPE_ROLE, ow0->type, "overwrite[0] type");
        TEST_ASSERT_EQ(8ULL, ow0->allow, "overwrite[0] allow");
        TEST_ASSERT_EQ(0ULL, ow0->deny, "overwrite[0] deny");

        const dc_permission_overwrite_t* ow1 = dc_vec_at(&channel.permission_overwrites, (size_t)1);
        TEST_ASSERT_NEQ(NULL, ow1, "overwrite[1] not null");
        TEST_ASSERT_EQ(500ULL, ow1->id, "overwrite[1] id");
        TEST_ASSERT_EQ(DC_PERMISSION_OVERWRITE_TYPE_MEMBER, ow1->type, "overwrite[1] type");
        TEST_ASSERT_EQ(0ULL, ow1->allow, "overwrite[1] allow default");
        TEST_ASSERT_EQ(4ULL, ow1->deny, "overwrite[1] deny");
    }

    TEST_ASSERT_EQ(DC_OK, dc_string_init(&result), "init channel overwrites to_json result");
    TEST_ASSERT_EQ(DC_OK, dc_channel_to_json(&channel, &result), "channel overwrites to json");
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(dc_string_cstr(&result), &doc),
                   "parse serialized channel overwrites");
    {
        yyjson_val* ovs = yyjson_obj_get(doc.root, "permission_overwrites");
        TEST_ASSERT_NEQ(NULL, ovs, "serialized permission_overwrites exists");
        TEST_ASSERT_EQ(2u, yyjson_arr_size(ovs), "serialized overwrites size");
    }
    dc_json_doc_free(&doc);
    dc_string_free(&result);
    dc_channel_free(&channel);

    const char* channel_missing_id = "{\"type\":0}";
    TEST_ASSERT_EQ(DC_OK, dc_channel_init(&channel), "init channel missing id");
    TEST_ASSERT_EQ(DC_ERROR_NOT_FOUND, dc_channel_from_json(channel_missing_id, &channel), "channel missing id");
    dc_channel_free(&channel);

    /* Model parsing: Message */
    const char* message_json =
        "{\"id\":\"999\",\"channel_id\":\"1000\",\"author\":{\"id\":\"123\",\"username\":\"alice\"},"
        "\"content\":\"hi\",\"timestamp\":\"2024-01-01T00:00:00.000Z\",\"tts\":false,"
        "\"mention_everyone\":false,\"pinned\":false,\"type\":0}";
    dc_message_t message;
    TEST_ASSERT_EQ(DC_OK, dc_message_init(&message), "init message");
    TEST_ASSERT_EQ(DC_OK, dc_message_from_json(message_json, &message), "parse message json");
    TEST_ASSERT_EQ(999ULL, message.id, "message id parsed");
    TEST_ASSERT_EQ(1000ULL, message.channel_id, "message channel id parsed");
    TEST_ASSERT_EQ(DC_MESSAGE_TYPE_DEFAULT, message.type, "message type parsed");
    TEST_ASSERT_STR_EQ("hi", dc_string_cstr(&message.content), "message content parsed");
    dc_message_free(&message);

    const char* message_components_legacy =
        "{\"id\":\"1001\",\"channel_id\":\"1002\",\"author\":{\"id\":\"123\",\"username\":\"alice\"},"
        "\"content\":\"legacy\",\"timestamp\":\"2024-01-01T00:00:00.000Z\",\"tts\":false,"
        "\"mention_everyone\":false,\"pinned\":false,\"type\":0,\"components\":["
        "{\"type\":1,\"components\":[{\"type\":2,\"custom_id\":\"click_me\",\"label\":\"Click\",\"style\":1}]}"
        "]}";
    TEST_ASSERT_EQ(DC_OK, dc_message_init(&message), "init message with legacy components");
    TEST_ASSERT_EQ(DC_OK, dc_message_from_json(message_components_legacy, &message),
                   "parse message with legacy components");
    TEST_ASSERT_EQ(1u, dc_vec_length(&message.components), "legacy top-level component count");
    const dc_component_t* legacy_row = dc_vec_at(&message.components, 0);
    TEST_ASSERT_EQ(DC_COMPONENT_TYPE_ACTION_ROW, legacy_row->type, "legacy action row type");
    TEST_ASSERT_EQ(1u, dc_vec_length(&legacy_row->components), "legacy action row child count");
    const dc_component_t* legacy_button = dc_vec_at(&legacy_row->components, 0);
    TEST_ASSERT_EQ(DC_COMPONENT_TYPE_BUTTON, legacy_button->type, "legacy button type");
    TEST_ASSERT_EQ(1, legacy_button->custom_id.is_set, "legacy button custom_id set");
    TEST_ASSERT_STR_EQ("click_me", dc_string_cstr(&legacy_button->custom_id.value),
                       "legacy button custom_id");
    TEST_ASSERT_EQ(1, legacy_button->style.is_set, "legacy button style set");
    TEST_ASSERT_EQ(1, legacy_button->style.value, "legacy button style value");
    dc_message_free(&message);

    const char* message_components_v2 =
        "{\"id\":\"2001\",\"channel_id\":\"2002\",\"author\":{\"id\":\"123\",\"username\":\"alice\"},"
        "\"content\":\"\",\"timestamp\":\"2025-04-22T00:00:00.000Z\",\"tts\":false,"
        "\"mention_everyone\":false,\"pinned\":false,\"type\":0,\"flags\":32768,\"components\":["
        "{\"type\":10,\"id\":7,\"content\":\"# Header\"},"
        "{\"type\":17,\"accent_color\":703487,\"components\":[{\"type\":10,\"content\":\"Inside container\"}]},"
        "{\"type\":12,\"items\":[{\"media\":{\"url\":\"https://example.com/a.png\"},\"description\":\"A\"}]}"
        "]}";
    TEST_ASSERT_EQ(DC_OK, dc_message_init(&message), "init message with v2 components");
    TEST_ASSERT_EQ(DC_OK, dc_message_from_json(message_components_v2, &message),
                   "parse message with v2 components");
    TEST_ASSERT_EQ(3u, dc_vec_length(&message.components), "v2 top-level component count");
    const dc_component_t* text_display = dc_vec_at(&message.components, 0);
    TEST_ASSERT_EQ(DC_COMPONENT_TYPE_TEXT_DISPLAY, text_display->type, "v2 text display type");
    TEST_ASSERT_EQ(1, text_display->content.is_set, "v2 text display content set");
    TEST_ASSERT_STR_EQ("# Header", dc_string_cstr(&text_display->content.value),
                       "v2 text display content");
    const dc_component_t* container_component = dc_vec_at(&message.components, 1);
    TEST_ASSERT_EQ(DC_COMPONENT_TYPE_CONTAINER, container_component->type, "v2 container type");
    TEST_ASSERT_EQ(1, container_component->accent_color.is_set, "v2 container accent color set");
    TEST_ASSERT_EQ(703487, container_component->accent_color.value, "v2 container accent color");
    TEST_ASSERT_EQ(1u, dc_vec_length(&container_component->components), "v2 container child count");
    const dc_component_t* gallery_component = dc_vec_at(&message.components, 2);
    TEST_ASSERT_EQ(DC_COMPONENT_TYPE_MEDIA_GALLERY, gallery_component->type, "v2 gallery type");
    TEST_ASSERT_EQ(1u, dc_vec_length(&gallery_component->items), "v2 gallery item count");
    const dc_media_gallery_item_t* gallery_item = dc_vec_at(&gallery_component->items, 0);
    TEST_ASSERT_STR_EQ("https://example.com/a.png", dc_string_cstr(&gallery_item->media.url),
                       "v2 gallery media url");

    dc_string_t serialized_message;
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&serialized_message), "init serialized message");
    TEST_ASSERT_EQ(DC_OK, dc_message_to_json(&message, &serialized_message),
                   "serialize message with components");
    TEST_ASSERT_EQ(DC_OK, dc_json_parse(dc_string_cstr(&serialized_message), &doc),
                   "parse serialized message with components");
    yyjson_val* serialized_components = yyjson_obj_get(doc.root, "components");
    TEST_ASSERT_NEQ(NULL, serialized_components, "serialized components field exists");
    TEST_ASSERT_EQ(3u, yyjson_arr_size(serialized_components), "serialized component count");
    yyjson_val* serialized_first = yyjson_arr_get(serialized_components, 0);
    int64_t serialized_first_type = 0;
    TEST_ASSERT_EQ(DC_OK, dc_json_get_int64(serialized_first, "type", &serialized_first_type),
                   "serialized first type parse");
    TEST_ASSERT_EQ(10, serialized_first_type, "serialized first type");
    TEST_ASSERT_STR_EQ("# Header", yyjson_get_str(yyjson_obj_get(serialized_first, "content")),
                       "serialized first content");
    dc_json_doc_free(&doc);
    dc_string_free(&serialized_message);
    dc_message_free(&message);

    const char* message_missing_id =
        "{\"channel_id\":\"1000\",\"author\":{\"id\":\"123\",\"username\":\"alice\"},"
        "\"content\":\"hi\",\"timestamp\":\"2024-01-01T00:00:00.000Z\",\"tts\":false,"
        "\"mention_everyone\":false,\"pinned\":false,\"type\":0}";
    TEST_ASSERT_EQ(DC_OK, dc_message_init(&message), "init message missing id");
    TEST_ASSERT_EQ(DC_ERROR_NOT_FOUND, dc_message_from_json(message_missing_id, &message), "message missing id");
    dc_message_free(&message);
    
    TEST_SUITE_END("JSON Tests");
}
