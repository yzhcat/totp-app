# TOTP App

一个纯C++17实现的TOTP (Time-based One-Time Password) 应用，提供CLI和TUI两种界面，符合RFC 6238标准。

## 特性

- **符合标准** - 完全实现RFC 6238 TOTP规范
- **多种算法支持** - 支持SHA1、SHA256、SHA512 HMAC算法
- **双界面模式** - 提供命令行(CLI)和文本用户界面(TUI)
- **跨平台** - 支持Windows、Linux、macOS
- **Header-only库** - 核心库可直接包含使用，无需编译
- **Google Authenticator兼容** - 支持标准otpauth://URL格式

## 项目结构

```
totp-app/
├── cli/                    # 命令行工具
│   ├── main.cpp           # CLI主程序
│   └── nob_cli.c          # CLI构建配置
├── tui/                    # 文本用户界面应用
│   ├── main.cpp           # TUI主程序
│   ├── ftxui/             # FTXUI库
│   └── nob_tui.c          # TUI构建配置
├── lib/                    # 核心库
│   ├── totp.hpp           # TOTP实现 (Header-only)
│   ├── otpauth.h          # OTPAuth URL解析器 (STB-style)
│   ├── base32.h           # Base32编解码
│   ├── hash-library/      # HMAC-SHA实现
│   │   ├── sha1.h/cpp
│   │   ├── sha256.h/cpp
│   │   ├── sha512.h/cpp
│   │   └── hmac.h
│   └── test/              # 测试文件
│       ├── test_totp.cpp
│       ├── test_base32.c
│       └── test_otpauth.c
├── nob.c                   # 主构建脚本
├── nob.h                   # 构建系统
└── README.md
```

## 依赖

- **编译器**: 支持C++17的编译器 (GCC, Clang, MSVC)
- **构建工具**: 无需额外构建系统（使用nob.h自包含构建）

## 快速开始

### 构建项目

```bash
# 编译构建工具
gcc nob.c -o nob

# 构建CLI工具
./nob cli

# 构建TUI应用
./nob tui

# 构建所有工具
./nob all

# 清理构建产物
./nob clean
```

Windows用户请将 `./nob` 替换为 `nob.exe`。

### 使用CLI工具

#### 基本用法

```bash
# 生成TOTP验证码
totp-cli "otpauth://totp/Example:alice@google.com?secret=JBSWY3DPEHPK3PXP"

# 从管道读取URL
echo "otpauth://totp/..." | totp-cli

# 从文件读取
head totp.cfg -n 1 | totp-cli
```

#### 高级选项

```bash
# 指定时间生成验证码
totp-cli "otpauth://totp/..." -t 14:30:00

# 使用时间戳
totp-cli "otpauth://totp/..." -t @1234567890

# 预览多个验证码
totp-cli "otpauth://totp/..." -p :3      # 当前+后3个
totp-cli "otpauth://totp/..." -p -1       # 前一个
totp-cli "otpauth://totp/..." -p -2:2     # 前2个到后2个

# 只输出验证码（便于脚本使用）
totp-cli "otpauth://totp/..." -s

# 显示剩余时间
totp-cli "otpauth://totp/..." -s -e
```

### 使用TUI应用

#### 配置文件

创建 `totp.cfg` 文件，每行一个otpauth URL：

```text
otpauth://totp/Example:alice@google.com?secret=JBSWY3DPEHPK3PXP&issuer=Example
otpauth://totp/GitHub:user@github.com?secret=ABCDEFGH&issuer=GitHub
# 注释行会被忽略
```

#### 启动TUI

```bash
# 使用默认配置文件 ./totp.cfg
totp-tui

# 指定配置文件
totp-tui -f /path/to/totp.cfg

# 设置预显示时间（默认5秒）
totp-tui -p 10

# 使用环境变量
export TOTP_CONFIG=/path/to/totp.cfg
export TOTP_PREVIEW=10
totp-tui
```

#### TUI操作

- **↑/↓**: 选择不同的TOTP条目
- **q/Esc**: 退出应用

## 核心库使用

### totp.hpp - Header-only TOTP库

```cpp
#define TOTP_IMPLEMENTATION
#include "totp.hpp"

// 从Base32密钥生成TOTP
uint32_t code = totp_from_b32(
    "JBSWY3DPEHPK3PXP",  // Base32编码的密钥
    TOTP_ALGO_SHA1,       // 算法：SHA1/SHA256/SHA512
    6,                    // 位数（6或8）
    30,                   // 时间步长（秒）
    time(nullptr)         // Unix时间戳
);

// 格式化为字符串
char code_str[9];
totp_code_str(code, 6, code_str);
printf("TOTP: %s\n", code_str);

// 计算剩余时间
int remaining = totp_remaining(30, time(nullptr));
```

### otpauth.h - URL解析器

```c
#define OTPAUTH_IMPLEMENTATION
#include "otpauth.h"

OTPAuthEntry entry = {};
if (otpauth_parse("otpauth://totp/Example:alice@google.com?secret=JBSWY3DPEHPK3PXP&issuer=Example", &entry)) {
    printf("Issuer: %s\n", entry.issuer);
    printf("Account: %s\n", entry.account);
    printf("Secret: %s\n", entry.secret_b32);
    printf("Algorithm: %s\n", entry.algorithm);
    printf("Digits: %d\n", entry.digits);
    printf("Period: %d\n", entry.period);
    otpauth_free(&entry);
}
```

## API参考

### TOTP函数

```cpp
// 计算TOTP（原始密钥）
uint32_t totp_compute(
    const void* secret,      // 密钥字节
    size_t secret_len,       // 密钥长度
    TOTPAlgorithm algo,      // 算法
    int digits,              // 位数（6或8）
    int period,              // 时间步长（秒）
    int64_t timestamp        // Unix时间戳
);

// 计算TOTP（Base32编码）
uint32_t totp_from_b32(
    const char* secret_b32,  // Base32密钥
    TOTPAlgorithm algo,
    int digits,
    int period,
    int64_t timestamp
);

// 格式化验证码为字符串
void totp_code_str(
    uint32_t code,
    int digits,
    char* out
);

// 计算剩余时间
int totp_remaining(
    int period,
    int64_t timestamp
);

// 获取指定偏移的时间戳
int64_t totp_step_timestamp(
    int period,
    int64_t timestamp,
    int offset
);
```

### 支持的算法

```cpp
TOTP_ALGO_SHA1   // SHA-1 (最常用)
TOTP_ALGO_SHA256 // SHA-256
TOTP_ALGO_SHA512 // SHA-512
```

## 测试

项目包含RFC 6238标准测试向量验证：

```bash
# 运行测试
cd lib/test
g++ -I.. -I../hash-library ../hash-library/sha1.cpp ../hash-library/sha256.cpp ../hash-library/sha512.cpp test_totp.cpp -o test_totp
./test_totp
```

## 技术细节

- **RFC 6238** - TOTP: Time-Based One-Time Password Algorithm
- **RFC 4226** - HOTP: An HMAC-Based One-Time Password Algorithm
- **Google Authenticator Key Uri Format** - otpauth:// URL格式

## 许可证

本项目采用MIT许可证。依赖库：
- hash-library: MIT License
- FTXUI: MIT License

## 贡献

欢迎提交Issue和Pull Request！

## 安全注意事项

此项目仅用于学习和研究目的，不建议在生产环境中使用。

- **不要分享你的密钥** - TOTP密钥等同于密码
- **安全存储配置文件** - tui 使用明文文件，建议在安全的环境中使用