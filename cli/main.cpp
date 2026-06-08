// totp-cli - TOTP 命令行工具
// 用法: totp-cli <otpauth_url> [-t <time>] [-p <preview_spec>]
#define VERSION "0.1.0"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <io.h>
    #define isatty _isatty
    #define STDIN_FILENO 0
#else
    #include <unistd.h>
#endif

// 跨平台时间函数封装
#if defined(_WIN32) || defined(_WIN64)
// Windows: localtime_s(struct tm*, time_t*)
// Linux:   localtime_r(time_t*, struct tm*)
static struct tm* localtime_r(const time_t* timep, struct tm* result) {
    if (localtime_s(result, timep) == 0) {
        return result;
    }
    return NULL;
}

// Windows 上没有 strptime，需要自己实现简单版本
static char* my_strptime(const char* s, const char* format, struct tm* tm) {
    // 支持格式: HH:mm:ss 和 yyyy-MM-dd HH:mm:ss
    if (strstr(format, "%Y") != NULL) {
        // yyyy-MM-dd HH:mm:ss 格式
        int year, month, day, hour, min, sec;
        if (sscanf(s, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) == 6) {
            tm->tm_year = year - 1900;
            tm->tm_mon = month - 1;
            tm->tm_mday = day;
            tm->tm_hour = hour;
            tm->tm_min = min;
            tm->tm_sec = sec;
            return (char*)(s + strlen(s));
        }
        return NULL;
    } else if (strstr(format, "%H") != NULL) {
        // HH:mm:ss 格式
        int hour, min, sec;
        if (sscanf(s, "%d:%d:%d", &hour, &min, &sec) == 3) {
            tm->tm_hour = hour;
            tm->tm_min = min;
            tm->tm_sec = sec;
            return (char*)(s + strlen(s));
        }
        return NULL;
    }
    return NULL;
}
#define strptime my_strptime
#endif

#define OTPAUTH_IMPLEMENTATION
#include "lib/otpauth.h"
#include "lib/totp.hpp"

// 时间解析函数
int64_t parse_time(const char* time_str) {
    if (!time_str) return time(NULL);
    
    // @timestamp 格式
    if (time_str[0] == '@') {
        return atoll(time_str + 1);
    }
    
    // "now" 格式
    if (strcmp(time_str, "now") == 0) {
        return time(NULL);
    }
    
    // 尝试解析 HH:mm:ss 格式
    struct tm tm = {};
    time_t now = time(NULL);
    localtime_r(&now, &tm);
    
    char* result = strptime(time_str, "%H:%M:%S", &tm);
    if (result && *result == '\0') {
        return mktime(&tm);
    }
    
    // 尝试解析 yyyy-MM-dd HH:mm:ss 格式
    result = strptime(time_str, "%Y-%m-%d %H:%M:%S", &tm);
    if (result && *result == '\0') {
        return mktime(&tm);
    }
    
    fprintf(stderr, "错误: 无法解析时间格式: %s\n", time_str);
    fprintf(stderr, "支持的格式: HH:mm:ss, yyyy-MM-dd HH:mm:ss, @timestamp, now\n");
    exit(1);
}

// 解析预览参数
void parse_preview(const char* preview_spec, int* start, int* end) {
    if (!preview_spec || !start || !end) return;
    
    *start = 0;
    *end = 0;
    
    // 检查是否包含冒号（范围格式）
    if (preview_spec[0] != ':' && strchr(preview_spec, ':')) {
        sscanf(preview_spec, "%d:%d", start, end);
    } else {
        // 单个数字
        int val;
        if (preview_spec[0]==':'){
            val = atoi(preview_spec+1);
        }else{
            val = atoi(preview_spec);
        }
        if (val >= 0) {
            // 正数等价于 :N
            *start = 0;
            *end = val;
        } else {
            // 负数表示单个偏移
            *start = val;
            *end = 0;
        }
    }
}

// 格式化时间字符串
void format_time(char* buf, size_t buf_size, int64_t timestamp) {
    time_t ts = (time_t)timestamp;
    struct tm tm;
    localtime_r(&ts, &tm);
    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", &tm);
}

// 打印 TOTP 条目信息
void print_entry_info(const OTPAuthEntry* entry) {
    printf("┌─ TOTP Entry ────────────────────────────────\n");
    printf("│ Type:      %-34s\n", entry->type ? entry->type : "N/A");
    printf("│ Issuer:    %-34s\n", entry->issuer ? entry->issuer : "N/A");
    printf("│ Account:   %-34s\n", entry->account ? entry->account : "N/A");
    printf("│ Algorithm: %-34s\n", entry->algorithm ? entry->algorithm : "N/A");
    printf("│ Digits:    %-34d\n", entry->digits);
    printf("│ Period:    %ds\n", entry->period);
    printf("└──────────────────────────────────────────────\n");
}

// 打印验证码
void print_code(const char* code_str, int digits, int remaining, int64_t timestamp, bool is_current) {    
    // 格式化验证码显示（每3位加空格）
    char formatted[20];
    int pos = 0;
    for (int i = 0; i < digits; i++) {
        if (i > 0 && i % 3 == 0) {
            formatted[pos++] = ' ';
        }
        formatted[pos++] = code_str[i];
    }
    formatted[pos] = '\0';
    
    char time_buf[32];
    format_time(time_buf, sizeof(time_buf), timestamp);
    
    if (is_current) {
        printf("Current Code: %s  (expires in %ds)\n", formatted, remaining);
    } else {
        printf("  %s\n", formatted);
    }
}

// 打印预览验证码
void print_preview_codes(const OTPAuthEntry* entry, int64_t base_timestamp, int start, int end, bool short_code) {
    TOTPAlgorithm algo = (TOTPAlgorithm)totp_parse_algorithm(entry->algorithm);
    for (int offset = start; offset <= end; offset++) {
        int64_t ts = totp_step_timestamp(entry->period, base_timestamp, offset);
        uint32_t code = totp_from_b32(entry->secret_b32, algo, entry->digits, entry->period, ts);
        
        char time_buf[32];
        format_time(time_buf, sizeof(time_buf), ts);
        char code_str[9];
        totp_code_str(code, entry->digits, code_str);
        if (short_code) {
            printf("%s%s\n", code_str, offset == 0 ? "*" : "");
            continue;
        }else{
            char marker[16];
            if (offset == 0) {
                snprintf(marker, sizeof(marker), "[ 0]");
            } else if (offset > 0) {
                snprintf(marker, sizeof(marker), "[+%d]", offset);
            } else {
                snprintf(marker, sizeof(marker), "[%2d]", offset);
            }
            printf("%s %s → %s %s\n", marker, time_buf, code_str, offset == 0 ? "← current" : "");
        }

    }
}

void print_usage(const char* prog_name,bool more_info=false) {
    printf("版本: %s\n", VERSION);
    printf("用法: %s [otpauth_url] [-t <time>] [-p <preview_spec>]\n", prog_name);
    printf("\n");
    printf("参数:\n");
    printf("  <otpauth_url>    otpauth://totp/... 格式的 URL（可从标准输入读取）\n");
    printf("  -t <time>        指定时间（默认当前时间）\n");
    printf("                   格式: HH:mm:ss, yyyy-MM-dd HH:mm:ss, @timestamp, now\n");
    printf("  -p <preview>     预览多个验证码\n");
    printf("                   格式: :n (当前+后n个), n (单个偏移), n1:n2 (范围)\n");
    printf("                   示例: -p :3, -p -1, -p -2:2\n");
    printf("  -h, --help       显示此帮助信息\n");
    printf("  -s, --short      只打印验证码\n");
    printf("\n");
    if (more_info) {
        printf("示例:\n");
        printf("  # 直接传入 URL\n");
        printf("  %s \"otpauth://totp/Example:alice@google.com?secret=JBSWY3DPEHPK3PXP\"\n", prog_name);
        printf("\n");
        printf("  # 从管道读取\n");
        printf("  echo \"otpauth://totp/...\" | %s -p -1:2\n", prog_name);
        printf("  head totp.cfg -n 1 | %s\n", prog_name);
        printf("\n");
        printf("  # 指定时间和预览\n");
        printf("  %s \"otpauth://totp/...\" -t 14:30:00 -p -1:2\n", prog_name);
    }
}

// 从标准输入读取一行 用于从管道读取 otpauth URL
char* read_stdin_line() {
    // 如果是终端，直接返回 NULL
    if (isatty(STDIN_FILENO)) return NULL;
    static char buffer[4096];
    
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        // 移除换行符
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
            len--;
        }
        
        // 移除回车符（Windows）
        if (len > 0 && buffer[len - 1] == '\r') {
            buffer[len - 1] = '\0';
            len--;
        }
        
        // 跳过空行和注释行
        if (len == 0 || buffer[0] == '#') {
            continue;
        }
        
        return buffer;
    }
    
    return NULL;
}

int main(int argc, char* argv[]) {
    const char* otpauth_url = NULL;
    const char* time_str = NULL;
    const char* preview_spec = NULL;
    int preview_start = 0, preview_end = 0;
    char* stdin_url = NULL;
    bool short_code = false; //-s 只打印验证码
    
    // 如果没有任何参数，直接显示帮助信息
    if (argc == 1) {
        if (isatty(STDIN_FILENO)) {
            print_usage(argv[0],true);
            return 0;
        }
    }
    
    // 跨平台参数解析
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            time_str = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            preview_spec = argv[++i];
            parse_preview(preview_spec, &preview_start, &preview_end);
            if (preview_start > preview_end || (preview_start == 0 && preview_end == 0)) {
                print_usage(argv[0]);
                fprintf(stderr, "错误: -p 参数范围无效\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-s") == 0) {
            short_code = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0],true);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("版本: %s\n", VERSION);
            return 0;
        } else if (argv[i][0] != '-') {
            otpauth_url = argv[i];
        } else {
            print_usage(argv[0]);
            fprintf(stderr, "错误: 无效参数: %s\n", argv[i]);
            return 1;
        }
    }

    // 如果没有提供 URL，尝试从标准输入读取
    if (!otpauth_url) {
        stdin_url = read_stdin_line();
        if (!stdin_url) {
            print_usage(argv[0]);
            fprintf(stderr, "错误: 缺少 otpauth URL 参数\n");
            return 1;
        }
        otpauth_url = stdin_url;
    }
    
    // 解析 otpauth URL
    OTPAuthEntry entry = {};
    if (!otpauth_parse(otpauth_url, &entry)) {
        fprintf(stderr, "错误: 无法解析 otpauth URL: %s\n", otpauth_url);
        return 1;
    }
    
    // 解析时间
    int64_t timestamp = parse_time(time_str);
    
    // 打印条目信息
    if (!short_code) {
        print_entry_info(&entry);
    }
    
    // 计算当前验证码
    TOTPAlgorithm algo = (TOTPAlgorithm)totp_parse_algorithm(entry.algorithm);
    uint32_t code = totp_from_b32(entry.secret_b32, algo, entry.digits, entry.period, timestamp);
    int remaining = totp_remaining(entry.period, timestamp);
    char code_str[9];
    totp_code_str(code, entry.digits, code_str);
    
    // 打印当前验证码
    if (short_code) {
        if (!preview_spec) printf("%s\n", code_str);
    } else {
        print_code(code_str, entry.digits, remaining, timestamp, true);
    }
    
    // 处理预览参数
    if (preview_spec) {
        print_preview_codes(&entry, timestamp, preview_start, preview_end, short_code);
    }
    
    // 清理
    otpauth_free(&entry);
    
    return 0;
}