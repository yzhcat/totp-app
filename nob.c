// nob.c - TOTP 项目构建脚本
// 使用 nob.h 构建系统
// gcc nob.c -o nob

#define NOB_IMPLEMENTATION
#define NOB_NO_ECHO
#include "nob.h"

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
    return nob_needs_rebuild("bin/totp-cli", input_paths, sizeof(input_paths)/sizeof(input_paths[0]));
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
    return nob_needs_rebuild("bin/totp-tui", input_paths, sizeof(input_paths)/sizeof(input_paths[0]));
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
    
    nob_cmd_append(&cmd, "-o", "bin/totp-cli");
    
    nob_log(INFO, "构建 CLI 工具...");
    bool result = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    
    if (result) {
        nob_log(INFO, "CLI 工具构建完成: bin/totp-cli");
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
    
    nob_cmd_append(&cmd, "-o", "bin/totp-tui");
    
    nob_log(INFO, "构建 TUI 应用...");
    bool result = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    
    if (result) {
        nob_log(INFO, "TUI 应用构建完成: bin/totp-tui");
    }
    return result;
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char *program = nob_shift_args(&argc, &argv);

    if (argc <= 0) {
        nob_log(ERROR, "用法: %s [cli|tui|all|clean]", program);
        return 1;
    }

    const char *command = nob_shift_args(&argc, &argv);

    if (strcmp(command, "clean") == 0) {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "rm", "-rf", "bin");
        nob_cmd_run_sync(cmd);
        nob_cmd_free(cmd);
        return 0;
    }


    nob_mkdir_if_not_exists("bin");

    if (strcmp(command, "cli") == 0 || strcmp(command, "all") == 0) {
        if (cli_needs_rebuild()) {
            build_cli();
        }
    }

    if (strcmp(command, "tui") == 0 || strcmp(command, "all") == 0) {
        if (tui_needs_rebuild()) {
            build_tui();
        }
    }

    if (strcmp(command, "cli") != 0 && strcmp(command, "tui") != 0 && 
        strcmp(command, "all") != 0 && strcmp(command, "clean") != 0) {
        nob_log(ERROR, "未知命令: %s", command);
        nob_log(ERROR, "支持的命令: cli, tui, all, clean");
        return 1;
    }

    return 0;
}