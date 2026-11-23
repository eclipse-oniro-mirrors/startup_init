/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <unistd.h>

#include "init_utils.h"  // INIT_STATIC: static under production, empty under STARTUP_INIT_TEST (ut)
#include "sysparam_errno.h"

#ifndef ENV_VALUE_BUF_LEN
#define ENV_VALUE_BUF_LEN 4096
#endif

// CLI takes an optional global --user <userid> flag; when omitted, -1 is passed
// to the SA so it derives userId from the calling token. userId=0 is rejected
// (hdc shells run as root with no HAP token, operator must pass --user).
static const int APPENV_USERID_FROM_TOKEN = -1;
static const int ARG_MIN_COUNT = 3;    // <prog> <action> <key>
static const int ARG_VALUE_OFFSET = 2;

// argv index/count constants for ParseUserOption/main. Named for clarity over
// raw indices in argv scanning.
static const int ARG_USER_FLAG_INDEX = 1;       // argv[1]: "--user"
static const int ARG_USER_VALUE_INDEX = 2;       // argv[2]: <userid>
static const int ARG_USER_MIN_COUNT = 5;         // <prog> --user <id> <action> <key>
static const int ACTION_START_DEFAULT = 1;       // action at argv[1] when no --user
static const int ACTION_START_WITH_USER = 3;     // action at argv[3] after --user <id>
static const int SET_ARG_COUNT = 3;              // <action> <key> <value>
static const int GET_UNSET_ARG_COUNT = 2;        // <action> <key>

#ifdef __cplusplus
extern "C" {
#endif
int SetEnvConfig(const char *key, const char *value, int userId);
int GetEnvConfig(const char *key, char *value, int size, int userId);
int RemoveEnvConfig(const char *key, int userId);
#ifdef __cplusplus
}
#endif

// Action type for HandleEnvResult: distinguishes the INVALID_INPUT message
// (set includes value format; get/unset only key format) and the success
// output. Keep in sync with main()'s action string match.
enum EnvAction {
    ACTION_SET,
    ACTION_GET,
    ACTION_UNSET,
};

// Error-code -> message mapping. Used for all non-zero SA return codes that
// share the same message across actions. Action-specific messages (INVALID_INPUT
// and NOT_FOUND which embed the key) are handled in PrintEnvError.
static const std::map<int, const char *> ERR_MAP = {
    { SYSPARAM_ENV_CONFIG_FULL,
        "env config full: max 256 entries reached\n" },
    { SYSPARAM_USERID_OUT_OF_RANGE,
        "userid out of range: --user must be in [100,999]\n" },
    { SYSPARAM_INVALID_USERID,
        "invalid userid: token-derived userid is 0 or undeducible; "
        "pass --user <userid> explicitly (must be in [100,999])\n" },
    { SYSPARAM_PERMISSION_DENIED,
        "permission denied: appenv requires root (callingUid == 0)\n" },
};

INIT_STATIC void PrintUsage(const char *prog)
{
    (void)fprintf(stderr,
        "Usage:\n"
        "  %s [--user <userid>] set <KEY> <VALUE>    Set environment variable\n"
        "  %s [--user <userid>] get <KEY>            Get environment variable\n"
        "  %s [--user <userid>] unset <KEY>          Remove environment variable\n",
        prog, prog, prog);
}

// Look up ret in ERR_MAP and print the message if matched. Returns 1 if the
// error code was handled (message printed), 0 if no match (caller prints the
// generic "<action> <key> failed: <ret>" fallback).
INIT_STATIC int PrintEnvError(int ret, enum EnvAction action, const char *key)
{
    auto it = ERR_MAP.find(ret);
    if (it != ERR_MAP.end()) {
        (void)fprintf(stderr, "%s", it->second);
        return 1;
    }
    if (ret == SYSPARAM_INVALID_INPUT) {
        // --user value is validated in ParseUserOption; INVALID_INPUT from SA
        // means key/value format failed. set also validates value, get/unset
        // only validate the key.
        if (action == ACTION_SET) {
            (void)fprintf(stderr, "invalid key or value format: key must be [A-Za-z_][A-Za-z0-9_]* (<=256 chars), "
                            "value must not contain \\n/\\r/\\0 (<=4095 chars)\n");
        } else {
            (void)fprintf(stderr, "invalid key format: key must be non-empty [A-Za-z_][A-Za-z0-9_]* (<=256 chars)\n");
        }
        return 1;
    }
    if (ret == SYSPARAM_NOT_FOUND) {
        // Surfacing the specific reason saves a round-trip to the SA logs.
        (void)fprintf(stderr, "key not found: %s\n", key);
        return 1;
    }
    return 0;  // unmatched — caller prints generic fallback
}

INIT_STATIC int HandleSetEnv(const char *key, const char *value, int userId)
{
    int ret = SetEnvConfig(key, value, userId);
    if (ret == 0) {
        printf("OK: %s=%s\n", key, value);
        return 0;
    }
    if (!PrintEnvError(ret, ACTION_SET, key)) {
        (void)fprintf(stderr, "set %s failed: %d\n", key, ret);
    }
    return 1;
}

INIT_STATIC int HandleGetEnv(const char *key, int userId)
{
    char value[ENV_VALUE_BUF_LEN] = {0};
    int ret = GetEnvConfig(key, value, sizeof(value), userId);
    if (ret == 0) {
        printf("%s\n", value);
        return 0;
    }
    if (!PrintEnvError(ret, ACTION_GET, key)) {
        (void)fprintf(stderr, "get %s failed: %d\n", key, ret);
    }
    return 1;
}

INIT_STATIC int HandleUnsetEnv(const char *key, int userId)
{
    int ret = RemoveEnvConfig(key, userId);
    if (ret == 0) {
        printf("OK: %s unset\n", key);
        return 0;
    }
    if (!PrintEnvError(ret, ACTION_UNSET, key)) {
        (void)fprintf(stderr, "unset %s failed: %d\n", key, ret);
    }
    return 1;
}

// Parse optional leading --user <userid>; default APPENV_USERID_FROM_TOKEN.
// On parse error prints usage and returns false (caller decides to exit/return).
// Sets actionStart to argv index of action; userId is an out parameter.
INIT_STATIC bool ParseUserOption(int argc, char *argv[], int &actionStart, int &userId)
{
    userId = APPENV_USERID_FROM_TOKEN;
    actionStart = ACTION_START_DEFAULT;
    if (argc > ARG_USER_FLAG_INDEX && strcmp(argv[ARG_USER_FLAG_INDEX], "--user") == 0) {
        if (argc < ARG_USER_MIN_COUNT) {
            PrintUsage(argv[0]);
            return false;
        }
        char *end = nullptr;
        long val = strtol(argv[ARG_USER_VALUE_INDEX], &end, 10);
        if (end == argv[ARG_USER_VALUE_INDEX] || *end != '\0') {
            (void)fprintf(stderr, "invalid --user value: %s\n", argv[ARG_USER_VALUE_INDEX]);
            return false;
        }
        userId = static_cast<int>(val);
        actionStart = ACTION_START_WITH_USER;
    }
    return true;
}

// Scan trailing argv for a misplaced global flag and emit a hint. Only --user
// is a known global flag; surfacing it explicitly from argc-mismatch paths
// helps the user spot the common "set KEY V --user N" mistake instead of
// relying on the user reading the generic usage text.
INIT_STATIC void EmitGlobalFlagHint(int argc, char *argv[], int actionStart)
{
    for (int i = actionStart + 1; i < argc; i++) {
        if (strcmp(argv[i], "--user") == 0) {
            (void)fprintf(stderr,
                "hint: --user is a global flag and must precede the subcommand\n");
            return;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < ARG_MIN_COUNT) {
        PrintUsage(argv[0]);
        return 1;
    }
    int actionStart = ACTION_START_DEFAULT;
    int userId = APPENV_USERID_FROM_TOKEN;
    if (!ParseUserOption(argc, argv, actionStart, userId)) {
        return 1;
    }
    if (argc - actionStart < GET_UNSET_ARG_COUNT) {  // need <action> <key>
        PrintUsage(argv[0]);
        return 1;
    }
    const char *action = argv[actionStart];
    const char *key = argv[actionStart + 1];
    if (strcmp(action, "set") == 0) {
        // Strict argc check catches missing quotes (e.g. `set CMD ls -la`)
        // and a misplaced --user after the subcommand. Without this, extra
        // argv slots are silently dropped and the user believes the call
        // succeeded with wrong data.
        if (argc - actionStart != SET_ARG_COUNT) {  // exactly <action> <key> <value>
            (void)fprintf(stderr, "set requires exactly KEY and VALUE\n");
            EmitGlobalFlagHint(argc, argv, actionStart);
            PrintUsage(argv[0]);
            return 1;
        }
        return HandleSetEnv(key, argv[actionStart + ARG_VALUE_OFFSET], userId);
    }
    if (strcmp(action, "get") == 0) {
        if (argc - actionStart != GET_UNSET_ARG_COUNT) {  // exactly <action> <key>
            (void)fprintf(stderr, "get requires exactly KEY\n");
            EmitGlobalFlagHint(argc, argv, actionStart);
            PrintUsage(argv[0]);
            return 1;
        }
        return HandleGetEnv(key, userId);
    }
    if (strcmp(action, "unset") == 0) {
        if (argc - actionStart != GET_UNSET_ARG_COUNT) {  // exactly <action> <key>
            (void)fprintf(stderr, "unset requires exactly KEY\n");
            EmitGlobalFlagHint(argc, argv, actionStart);
            PrintUsage(argv[0]);
            return 1;
        }
        return HandleUnsetEnv(key, userId);
    }
    PrintUsage(argv[0]);
    return 1;
}
