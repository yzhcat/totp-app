#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define isatty _isatty
#define STDIN_FILENO 0
#else
#include <unistd.h>
#endif

#include "ftxui/ftxui.hpp"
#define TOTP_IMPLEMENTATION
#define OTPAUTH_IMPLEMENTATION
#include "lib/totp.hpp"
#include "lib/otpauth.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <ctime>
#include <chrono>
#include <thread>
#include <atomic>

struct TOTPEntry {
    OTPAuthEntry otp;
    uint32_t current_code;
    uint32_t next_code;
    int remaining;
    bool show_next;
};

std::vector<TOTPEntry> entries;
int selected_index = 0;
std::string config_path = "./totp.cfg";
int preview_seconds = 5;
std::atomic<bool> running{true};

std::string get_env_var(const char* name, const std::string& default_val) {
    const char* val = std::getenv(name);
    return val ? std::string(val) : default_val;
}

void parse_config(const std::string& path) {
    entries.clear();
    std::ifstream file(path);
    if (!file.is_open()) {
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        OTPAuthEntry entry = {};
        if (otpauth_parse(line.c_str(), &entry)) {
            entries.push_back({entry, 0, 0, 0, false});
        }
    }
}

void update_codes() {
    int64_t now = std::time(nullptr);
    for (auto& entry : entries) {
        TOTPAlgorithm algo = (TOTPAlgorithm)totp_parse_algorithm(entry.otp.algorithm);
        entry.current_code = totp_from_b32(
            entry.otp.secret_b32,
            algo,
            entry.otp.digits,
            entry.otp.period,
            now
        );
        entry.remaining = totp_remaining(entry.otp.period, now);
        entry.show_next = entry.remaining <= preview_seconds;
        if (entry.show_next) {
            int64_t next_time = totp_step_timestamp(entry.otp.period, now, 1);
            entry.next_code = totp_from_b32(
                entry.otp.secret_b32,
                algo,
                entry.otp.digits,
                entry.otp.period,
                next_time
            );
        }
    }
}

std::string format_code(uint32_t code, int digits) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%0*d", digits, code);
    std::string s(buf);
    for (size_t i = 3; i < s.size(); i += 4) {
        s.insert(i, " ");
    }
    return s;
}

uint32_t end_code = 0;

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            preview_seconds = std::stoi(argv[++i]);
        }
    }

    std::string env_path = get_env_var("TOTP_CONFIG", "");
    if (!env_path.empty() && config_path == "./totp.cfg") {
        config_path = env_path;
    }
    std::string env_preview = get_env_var("TOTP_PREVIEW", "");
    if (!env_preview.empty()) {
        preview_seconds = std::stoi(env_preview);
    }

    parse_config(config_path);
    if (entries.empty()) {
        std::cerr << "No entries found in " << config_path << std::endl;
        return 1;
    }

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    ftxui::Component list = ftxui::Container::Vertical({});
    auto renderer = ftxui::Renderer(list, [&]() {
        update_codes();
        
        ftxui::Elements items;
        for (size_t i = 0; i < entries.size(); i++) {
            auto& entry = entries[i];
            std::string issuer = entry.otp.issuer ? entry.otp.issuer : "N/A";
            std::string account = entry.otp.account ? entry.otp.account : "N/A";
            std::string label = issuer + " - " + account;
            auto style = static_cast<int>(i) == selected_index 
                ? ftxui::color(ftxui::Color::Blue) | ftxui::bold 
                : ftxui::color(ftxui::Color::White);
            items.push_back(ftxui::text(label) | style);
        }
        
        auto list_render = ftxui::vbox(items) | ftxui::vscroll_indicator | ftxui::frame;

        ftxui::Elements detail;
        if (!entries.empty() && selected_index >= 0 && selected_index < static_cast<int>(entries.size())) {
            auto& entry = entries[selected_index];
            detail.push_back(ftxui::text("Issuer: " + std::string(entry.otp.issuer ? entry.otp.issuer : "N/A")));
            detail.push_back(ftxui::text("Account: " + std::string(entry.otp.account ? entry.otp.account : "N/A")));
            detail.push_back(ftxui::text("Algorithm: " + std::string(entry.otp.algorithm ? entry.otp.algorithm : "SHA1")));
            detail.push_back(ftxui::text("Digits: " + std::to_string(entry.otp.digits)));
            detail.push_back(ftxui::text("Period: " + std::to_string(entry.otp.period) + "s"));
            detail.push_back(ftxui::text(""));
            detail.push_back(ftxui::text("Current Code:") | ftxui::bold);
            detail.push_back(ftxui::text("  " + format_code(entry.current_code, entry.otp.digits)) | 
                           ftxui::bold | ftxui::color(ftxui::Color::Green));
            detail.push_back(ftxui::text("Remaining: " + std::to_string(entry.remaining) + "s") |
                           (entry.remaining <= preview_seconds ? ftxui::color(ftxui::Color::Red) : ftxui::color(ftxui::Color::White)));
            
            if (entry.show_next) {
                detail.push_back(ftxui::text(""));
                detail.push_back(ftxui::text("Next Code:") | ftxui::bold | ftxui::color(ftxui::Color::Yellow));
                detail.push_back(ftxui::text("  " + format_code(entry.next_code, entry.otp.digits)) | 
                               ftxui::bold | ftxui::color(ftxui::Color::Yellow) | ftxui::blink);
            }
        }
        
        auto detail_render = ftxui::vbox(detail) | ftxui::border | ftxui::flex;

        return ftxui::hbox({
            list_render | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 50),
            detail_render | ftxui::flex
        });
    });

    renderer |= ftxui::CatchEvent([&](ftxui::Event event) {
        if (event == ftxui::Event::ArrowUp) {
            selected_index = ((0 > (selected_index - 1)) ? 0 : (selected_index - 1));
            return true;
        }
        if (event == ftxui::Event::ArrowDown) {
            selected_index = ((static_cast<int>(entries.size()) - 1 < selected_index + 1) ? (static_cast<int>(entries.size()) - 1) : (selected_index + 1));
            return true;
        }
        if (event == ftxui::Event::Character('q') || event == ftxui::Event::Escape) {
            running = false;
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == ftxui::Event::Return) {
            if (!entries.empty() && selected_index >= 0 && selected_index < static_cast<int>(entries.size())) {
                auto& entry = entries[selected_index];
                end_code = entry.current_code;
            }
            running = false;
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    std::thread update_thread([&]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            screen.PostEvent(ftxui::Event::Custom);
        }
    });

    screen.Loop(renderer);
    running = false;
    update_thread.join();

    for (auto& entry : entries) {
        otpauth_free(&entry.otp);
    }

    if (end_code != 0) {
        std::cout << end_code << std::endl;
    }
    return 0;
}
