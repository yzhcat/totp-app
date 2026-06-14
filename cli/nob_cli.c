// nob.c - TOTP 项目构建脚本
// 使用 nob.h 构建系统
// gcc nob_cli.c -o nob_cli && ./nob_cli

#define NOB_IMPLEMENTATION
#include "../nob.h"

// 输出文件名
#if defined(_WIN32) || defined(_WIN64)
#define CLI_OUTPUT_PATH "totp-cli.exe"
#else
#define CLI_OUTPUT_PATH "totp-cli.bin"
#endif

// 检查是否需要重新构建
static int cli_needs_rebuild(void) {
    const char *input_paths[] = {
        "main.cpp",
        "../lib/totp.hpp",
        "../lib/otpauth.h",
        "../lib/base32.h",
        "../lib/hash-library/hmac.h",
        "../lib/hash-library/sha1.cpp",
        "../lib/hash-library/sha256.cpp",
        "../lib/hash-library/sha512.cpp"
    };
    return nob_needs_rebuild(CLI_OUTPUT_PATH, input_paths, sizeof(input_paths)/sizeof(input_paths[0]));
}


// 构建 CLI 工具
static bool build_cli(void) {
    Nob_Cmd cmd = {0};
    
    nob_cmd_append(&cmd, "g++");
    nob_cmd_append(&cmd, "-std=c++17");
    nob_cmd_append(&cmd, "-Wall", "-Wextra", "-O2");
    nob_cmd_append(&cmd, "-I..");
    
    nob_cmd_append(&cmd, "main.cpp");
    nob_cmd_append(&cmd, "../lib/hash-library/sha1.cpp");
    nob_cmd_append(&cmd, "../lib/hash-library/sha256.cpp");
    nob_cmd_append(&cmd, "../lib/hash-library/sha512.cpp");
    
    nob_cmd_append(&cmd, "-o", CLI_OUTPUT_PATH);
    
    nob_log(INFO, "Building CLI tool...");
    bool result = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    
    if (result) {
        nob_log(INFO, "CLI tool built: %s", CLI_OUTPUT_PATH);
    }
    return result;
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char *command = nob_shift_args(&argc, &argv);

    if (cli_needs_rebuild()) {
        build_cli();
    }

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, CLI_OUTPUT_PATH);
    if (argc > 0) {
        for (int i = 0; i < argc; i++) {
            nob_cmd_append(&cmd, argv[i]);
        }
    }
    if (!nob_cmd_run(&cmd, NULL)) return 1;
    return 0;
}
