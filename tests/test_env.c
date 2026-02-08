/**
 * @file test_env.c
 * @brief Dotenv/env loader tests
 */

#if defined(__unix__) || defined(__APPLE__)
/* Expose setenv/unsetenv/mkdtemp prototypes on glibc. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "test_utils.h"
#include "core/dc_env.h"
#include "core/dc_string.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static int write_text_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (!f) return 0;
    fputs(content ? content : "", f);
    fclose(f);
    return 1;
}

static void env_set(const char* key, const char* val) {
    if (!key) return;
#if defined(_WIN32)
    /* Best-effort; empty string behaves like "unset" for dc_env_get_process(). */
    _putenv_s(key, val ? val : "");
#else
    setenv(key, val ? val : "", 1);
#endif
}

static void env_unset(const char* key) {
    if (!key) return;
#if defined(_WIN32)
    _putenv_s(key, "");
#else
    unsetenv(key);
#endif
}

int test_env_main(void) {
    TEST_SUITE_BEGIN("Env/Dotenv Tests");

    dc_string_t out;
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&out), "init out");

#if defined(__unix__) || defined(__APPLE__)
    /* Create temp dir */
    char tmp_template[] = "/tmp/dc_env_testXXXXXX";
    char* tmp_dir = mkdtemp(tmp_template);
    TEST_ASSERT_NOT_NULL(tmp_dir, "mkdtemp");
    if (!tmp_dir) {
        dc_string_free(&out);
        TEST_SUITE_END("Env/Dotenv Tests");
    }

    char env_path[1024];
    snprintf(env_path, sizeof(env_path), "%s/.env", tmp_dir);

    const char* dotenv_content =
        "# comment line\n"
        "export SIMPLE=hello\n"
        "QUOTED=\"hello world\"  # trailing comment\n"
        "HASH_UNQUOTED=abc # comment\n"
        "HASH_QUOTED=\"abc # not comment\" # comment\n"
        "ESCAPES=\"a\\n\\t\\\\b\" # comment\n";
    TEST_ASSERT(write_text_file(env_path, dotenv_content) == 1, "write dotenv file");

    /* Permission enforcement */
    chmod(env_path, (mode_t)0644);
    TEST_ASSERT_EQ(DC_ERROR_FORBIDDEN,
                   dc_env_get_from_file(env_path, "SIMPLE", DC_ENV_FLAG_REQUIRE_PRIVATE_FILE, &out),
                   "require private forbids 0644");
    chmod(env_path, (mode_t)0600);

    TEST_ASSERT_EQ(DC_OK, dc_env_get_from_file(env_path, "SIMPLE", 0u, &out), "get SIMPLE");
    TEST_ASSERT_STR_EQ("hello", dc_string_cstr(&out), "SIMPLE value");

    TEST_ASSERT_EQ(DC_OK, dc_env_get_from_file(env_path, "QUOTED", 0u, &out), "get QUOTED");
    TEST_ASSERT_STR_EQ("hello world", dc_string_cstr(&out), "QUOTED value");

    TEST_ASSERT_EQ(DC_OK, dc_env_get_from_file(env_path, "HASH_UNQUOTED", 0u, &out), "get HASH_UNQUOTED");
    TEST_ASSERT_STR_EQ("abc", dc_string_cstr(&out), "HASH_UNQUOTED value");

    TEST_ASSERT_EQ(DC_OK, dc_env_get_from_file(env_path, "HASH_QUOTED", 0u, &out), "get HASH_QUOTED");
    TEST_ASSERT_STR_EQ("abc # not comment", dc_string_cstr(&out), "HASH_QUOTED value");

    TEST_ASSERT_EQ(DC_OK, dc_env_get_from_file(env_path, "ESCAPES", 0u, &out), "get ESCAPES");
    TEST_ASSERT_STR_EQ("a\n\t\\b", dc_string_cstr(&out), "ESCAPES unescaped");

    /* Tilde expansion for file paths */
    const char* old_home = getenv("HOME");
    env_set("HOME", tmp_dir);
    TEST_ASSERT_EQ(DC_OK, dc_env_get_from_file("~/.env", "SIMPLE", 0u, &out), "tilde path expansion");
    TEST_ASSERT_STR_EQ("hello", dc_string_cstr(&out), "tilde SIMPLE value");
    if (old_home) env_set("HOME", old_home);
    else env_unset("HOME");

    /* Auto search: walk up from nested dirs */
    char nested_a[1024];
    char nested_b[1024];
    char nested_c[1024];
    snprintf(nested_a, sizeof(nested_a), "%s/a", tmp_dir);
    snprintf(nested_b, sizeof(nested_b), "%s/a/b", tmp_dir);
    snprintf(nested_c, sizeof(nested_c), "%s/a/b/c", tmp_dir);
    mkdir(nested_a, (mode_t)0700);
    mkdir(nested_b, (mode_t)0700);
    mkdir(nested_c, (mode_t)0700);

    char cwd_buf[1024];
    char* old_cwd = getcwd(cwd_buf, sizeof(cwd_buf));
    TEST_ASSERT_NOT_NULL(old_cwd, "getcwd for restore");
    TEST_ASSERT_EQ(0, chdir(nested_c), "chdir nested");

    env_unset("AUTO_KEY");
    TEST_ASSERT_EQ(DC_OK,
                   dc_env_get_with_dotenv_search("SIMPLE", ".env", (size_t)10, 0u, &out),
                   "auto search SIMPLE from parents");
    TEST_ASSERT_STR_EQ("hello", dc_string_cstr(&out), "auto search SIMPLE value");

    if (old_cwd) (void)chdir(old_cwd);

    /* DC_DOTENV_PATH override for auto search */
    env_set("DC_DOTENV_PATH", env_path);
    TEST_ASSERT_EQ(DC_OK,
                   dc_env_get_with_dotenv_search("QUOTED", ".env", (size_t)0, 0u, &out),
                   "DC_DOTENV_PATH override");
    TEST_ASSERT_STR_EQ("hello world", dc_string_cstr(&out), "DC_DOTENV_PATH value");
    env_unset("DC_DOTENV_PATH");

    /* Load dotenv into process env */
    env_unset("DC_ENV_TEST_ONE");
    env_set("DC_ENV_TEST_KEEP", "orig");

    const char* load_content =
        "DC_ENV_TEST_ONE=1\n"
        "DC_ENV_TEST_KEEP=new\n"
        "DC_ENV_TEST_EMPTY=\n";
    TEST_ASSERT(write_text_file(env_path, load_content) == 1, "write load dotenv file");
    chmod(env_path, (mode_t)0600);

    size_t loaded = 0;
    TEST_ASSERT_EQ(DC_OK, dc_env_load_dotenv(env_path, 0u, &loaded), "load dotenv");
    TEST_ASSERT(loaded >= 1u, "loaded count >= 1");
    TEST_ASSERT_STR_EQ("1", getenv("DC_ENV_TEST_ONE"), "load set ONE");
    TEST_ASSERT_STR_EQ("orig", getenv("DC_ENV_TEST_KEEP"), "load does not override by default");
    TEST_ASSERT(getenv("DC_ENV_TEST_EMPTY") == NULL, "load skips empty by default");

    loaded = 0;
    TEST_ASSERT_EQ(DC_OK,
                   dc_env_load_dotenv(env_path, DC_ENV_FLAG_OVERRIDE_EXISTING | DC_ENV_FLAG_ALLOW_EMPTY, &loaded),
                   "load dotenv override+allow_empty");
    TEST_ASSERT_STR_EQ("new", getenv("DC_ENV_TEST_KEEP"), "load override existing");
    TEST_ASSERT_NOT_NULL(getenv("DC_ENV_TEST_EMPTY"), "load sets empty when allowed");
    TEST_ASSERT_STR_EQ("", getenv("DC_ENV_TEST_EMPTY"), "load empty value");

#else
    /* Non-POSIX: basic process env test only. */
    env_set("DC_ENV_TEST_PROCESS", "x");
    TEST_ASSERT_EQ(DC_OK, dc_env_get_process("DC_ENV_TEST_PROCESS", &out), "get process env");
    TEST_ASSERT_STR_EQ("x", dc_string_cstr(&out), "process value");
    env_unset("DC_ENV_TEST_PROCESS");
#endif

    dc_string_free(&out);
    TEST_SUITE_END("Env/Dotenv Tests");
}
