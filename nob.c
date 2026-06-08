// nob.c - TOTP 项目构建脚本
// 使用 nob.h 构建系统
// gcc nob.c -o nob

#define NOB_IMPLEMENTATION
#include "nob.h"

// 输出文件路径
#define OUTPUT_DIR "bin"
// 输出文件名
#if defined(_WIN32) || defined(_WIN64)
#define CLI_OUTPUT_PATH "bin/totp-cli.exe"
#define TUI_OUTPUT_PATH "bin/totp-tui.exe"
#else
#define CLI_OUTPUT_PATH "bin/totp-cli.bin"
#define TUI_OUTPUT_PATH "bin/totp-tui.bin"
#endif

// clean 命令
#if defined(_WIN32) || defined(_WIN64)
#define CLEAN_CMD "cmd", "/c", "rmdir", "/s/q"
#else
#define CLEAN_CMD "rm", "-rf"
#endif

// 检查是否需要重新构建
static int cli_needs_rebuild(void) {
    const char *input_paths[] = {
        "cli/main.cpp",
        "lib/totp.hpp",
        "lib/otpauth.h",
        "lib/base32.h",
        "hash-library/hmac.h",
        "hash-library/sha1.cpp",
        "hash-library/sha256.cpp",
        "hash-library/sha512.cpp"
    };
    return nob_needs_rebuild(CLI_OUTPUT_PATH, input_paths, sizeof(input_paths)/sizeof(input_paths[0]));
}

static int tui_needs_rebuild(void) {
    const char *input_paths[] = {
        "tui/main.cpp",
        "lib/totp.hpp",
        "lib/otpauth.h",
        "lib/base32.h",
        "hash-library/hmac.h",
        "hash-library/sha1.cpp",
        "hash-library/sha256.cpp",
        "hash-library/sha512.cpp"
    };
    return nob_needs_rebuild(TUI_OUTPUT_PATH, input_paths, sizeof(input_paths)/sizeof(input_paths[0]));
}

// 构建 CLI 工具
static bool build_cli(void) {
    Nob_Cmd cmd = {0};
    
    nob_cmd_append(&cmd, "g++");
    nob_cmd_append(&cmd, "-std=c++17");
    nob_cmd_append(&cmd, "-Wall", "-Wextra", "-O2");
    nob_cmd_append(&cmd, "-I.");
    nob_cmd_append(&cmd, "-Ihash-library");
    
    nob_cmd_append(&cmd, "cli/main.cpp");
    nob_cmd_append(&cmd, "hash-library/sha1.cpp");
    nob_cmd_append(&cmd, "hash-library/sha256.cpp");
    nob_cmd_append(&cmd, "hash-library/sha512.cpp");
    
    nob_cmd_append(&cmd, "-o", CLI_OUTPUT_PATH);
    
    nob_log(INFO, "Building CLI tool...");
    bool result = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    
    if (result) {
        nob_log(INFO, "CLI tool built: %s", CLI_OUTPUT_PATH);
    }
    return result;
}

// 构建 TUI 应用
static bool build_tui(void) {
    Nob_Cmd cmd = {0};
    
    nob_cmd_append(&cmd, "g++");
    nob_cmd_append(&cmd, "-std=c++17");
    nob_cmd_append(&cmd, "-Wall", "-Wextra", "-O2");
    nob_cmd_append(&cmd, "-I.");
    nob_cmd_append(&cmd, "-Ihash-library");
    nob_cmd_append(&cmd, "-pthread");
    
    nob_cmd_append(&cmd, "tui/main.cpp");
    nob_cmd_append(&cmd, "hash-library/sha1.cpp");
    nob_cmd_append(&cmd, "hash-library/sha256.cpp");
    nob_cmd_append(&cmd, "hash-library/sha512.cpp");
    
    nob_cmd_append(&cmd, "-o", TUI_OUTPUT_PATH);
    
    nob_log(INFO, "Building TUI application...");
    bool result = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    
    if (result) {
        nob_log(INFO, "TUI application built: %s", TUI_OUTPUT_PATH);
    }
    return result;
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char *program = nob_shift_args(&argc, &argv);

    if (argc <= 0) {
        nob_log(ERROR, "Usage: %s [cli|tui|all|clean]", program);
        return 1;
    }

    const char *command = nob_shift_args(&argc, &argv);

    if (strcmp(command, "clean") == 0) {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, CLEAN_CMD, OUTPUT_DIR);
        nob_cmd_run_sync(cmd);
        nob_cmd_free(cmd);
        nob_log(INFO, "clean completed: %s", OUTPUT_DIR);
        return 0;
    }

    if(!file_exists(OUTPUT_DIR)){
        nob_mkdir_if_not_exists(OUTPUT_DIR);
    }

    char RUN_PATH[256];

    if (strcmp(command, "cli") == 0 || strcmp(command, "all") == 0) {
        if (cli_needs_rebuild()) {
            build_cli();
        }
        strcpy(RUN_PATH, CLI_OUTPUT_PATH);
    }

    if (strcmp(command, "tui") == 0 || strcmp(command, "all") == 0) {
        if (tui_needs_rebuild()) {
            build_tui();
        }
        strcpy(RUN_PATH, TUI_OUTPUT_PATH);
    }

    if (strcmp(command, "cli") != 0 && strcmp(command, "tui") != 0 && 
        strcmp(command, "all") != 0 && strcmp(command, "clean") != 0) {
        nob_log(ERROR, "Unknown command: %s", command);
        nob_log(ERROR, "Supported commands: cli, tui, all, clean");
        return 1;
    }
    // Run the test
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, RUN_PATH);
    if (!nob_cmd_run(&cmd, NULL)) return 1;
    return 0;
}