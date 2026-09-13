/**
 * @file main.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Entry point for project::iq_forge. Reads manifest.env + spi.json
 *        from the current directory, loads the FPGA bitstream, applies the
 *        device-tree overlay, brings up the AD9361, then opens an
 *        interactive numbered menu to control TX frequency/attenuation/AGC,
 *        TX and DDS enable, and read back live hardware state. Meant to be
 *        deployed to the target board (see scripts/deploy.sh,
 *        scripts/load.sh) - a plain `./iq_forge_app` with no arguments does
 *        the whole thing, no flags needed on target.
 * @version 0.2
 * @date 2026-09-12
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "iq_forge.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>

#include <csignal>
#include <termios.h>
#include <unistd.h>

static void usage(const char *prog) {
    std::fprintf(stderr,
        "Usage: %s [--help]\n"
        "  Reads manifest.env + spi.json from the current directory, loads\n"
        "  the FPGA bitstream, applies the device-tree overlay, brings up\n"
        "  the AD9361, then opens an interactive numbered menu to read/set\n"
        "  TX frequency and attenuation, RX AGC mode, TX enable/disable,\n"
        "  DDS enable/disable/frequency/reset, and read back live hardware\n"
        "  state.\n",
        prog);
}

static const char *ensm_state_name(drivers::ensm_state state) {
    switch (state) {
        case drivers::ensm_state::sleep_wait: return "sleep_wait";
        case drivers::ensm_state::alert: return "alert (tx/rx idle)";
        case drivers::ensm_state::tx: return "tx";
        case drivers::ensm_state::tx_flush: return "tx_flush";
        case drivers::ensm_state::rx: return "rx";
        case drivers::ensm_state::rx_flush: return "rx_flush";
        case drivers::ensm_state::fdd: return "fdd (tx+rx active)";
        case drivers::ensm_state::fdd_flush: return "fdd_flush";
        case drivers::ensm_state::sleep: return "sleep";
        default: return "invalid";
    }
}

static std::optional<std::map<std::string, std::string>> read_manifest(const std::string &path) {
    std::ifstream in(path);
    if (!in) {
        return std::nullopt;
    }

    std::map<std::string, std::string> result;
    std::string line;
    while (std::getline(in, line)) {
        std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        result[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return result;
}

static const std::string *manifest_get(const std::map<std::string, std::string> &manifest, const char *key) {
    auto it = manifest.find(key);
    if (it == manifest.end()) {
        return nullptr;
    }
    return &it->second;
}

static std::optional<std::uint64_t> parse_u64(const std::string &s) {
    if (s.empty()) {
        return std::nullopt;
    }
    char *end = nullptr;
    unsigned long long v = std::strtoull(s.c_str(), &end, 0);
    if (end != s.c_str() + s.size()) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(v);
}

// Reads one line from stdin. nullopt means EOF (e.g. ssh running this
// non-interactively, or the user hit ctrl-D) - callers treat that as "leave".
static std::optional<std::string> read_line(const char *prompt) {
    if (prompt) {
        std::fputs(prompt, stdout);
        std::fflush(stdout);
    }
    std::string line;
    if (!std::getline(std::cin, line)) {
        return std::nullopt;
    }
    return line;
}

// Prompts until a number in [min, max] is entered. nullopt on EOF.
static std::optional<long> read_choice(const char *prompt, long min, long max) {
    for (;;) {
        auto line = read_line(prompt);
        if (!line) {
            return std::nullopt;
        }
        if (line->empty()) {
            continue;
        }
        char *end = nullptr;
        long v = std::strtol(line->c_str(), &end, 10);
        if (end != line->c_str() + line->size()) {
            std::printf("enter a number\n");
            continue;
        }
        if (v < min || v > max) {
            std::printf("enter a number between %ld and %ld\n", min, max);
            continue;
        }
        return v;
    }
}

static std::optional<std::uint64_t> read_u64(const char *prompt) {
    auto line = read_line(prompt);
    if (!line) {
        return std::nullopt;
    }
    return parse_u64(*line);
}

static std::optional<double> read_double(const char *prompt) {
    auto line = read_line(prompt);
    if (!line || line->empty()) {
        return std::nullopt;
    }
    char *end = nullptr;
    double v = std::strtod(line->c_str(), &end);
    if (end != line->c_str() + line->size()) {
        return std::nullopt;
    }
    return v;
}

// Sub-menus. Each returns nullopt on "0) back" or EOF - callers just abandon
// the action in that case rather than distinguishing the two.

static std::optional<drivers::rx_gain_mode> select_agc_mode() {
    std::printf("\n 1) manual\n 2) fast attack agc\n 3) slow attack agc\n 4) hybrid agc\n 0) back\n");
    auto choice = read_choice("> ", 0, 4);
    if (!choice || *choice == 0) {
        return std::nullopt;
    }
    switch (*choice) {
        case 1: return drivers::rx_gain_mode::manual;
        case 2: return drivers::rx_gain_mode::fast_attack_agc;
        case 3: return drivers::rx_gain_mode::slow_attack_agc;
        default: return drivers::rx_gain_mode::hybrid_agc;
    }
}

static std::optional<bool> select_on_off() {
    std::printf("\n 1) on\n 2) off\n 0) back\n");
    auto choice = read_choice("> ", 0, 2);
    if (!choice || *choice == 0) {
        return std::nullopt;
    }
    return *choice == 1;
}

// Single-keypress raw terminal mode for the delay tuner below - no Enter
// needed between +/-/s/q. ISIG stays on so Ctrl-C still works; a SIGINT
// handler restores the terminal before the process dies so an interrupted
// tuning session doesn't leave the SSH session's tty stuck echo-less.
static struct termios g_orig_termios;
static bool g_raw_mode_active = false;

static void disable_raw_mode() {
    if (g_raw_mode_active) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
        g_raw_mode_active = false;
    }
}

static void handle_sigint_in_raw_mode(int sig) {
    disable_raw_mode();
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

static bool enable_raw_mode() {
    if (!isatty(STDIN_FILENO)) {
        return false;
    }
    if (tcgetattr(STDIN_FILENO, &g_orig_termios) != 0) {
        return false;
    }
    struct termios raw = g_orig_termios;
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        return false;
    }
    g_raw_mode_active = true;
    std::signal(SIGINT, handle_sigint_in_raw_mode);
    return true;
}

// Blocks for one keypress, no Enter required. Returns -1 on EOF/read error.
static int read_key() {
    unsigned char c = 0;
    if (::read(STDIN_FILENO, &c, 1) != 1) {
        return -1;
    }
    return c;
}

// Live +/- tuning of one 0-15 field of REG_TX_CLOCK_DATA_DELAY, writing to
// hardware on every keypress so the effect (spectrum, ILA, whatever the
// user is watching) shows up immediately. 's' keeps the current value and
// returns; 'q'/Esc reverts to whatever the register held on entry.
static void tune_tx_clock_data_delay_field(project::iq_forge &forge, const char *field_name, bool tune_fb_clk) {
    std::uint8_t fb = 0, td = 0;
    if (!forge.get_ad9361_tx_clock_data_delay(fb, td)) {
        std::printf("error: get tx clock/data delay failed (%d)\n", forge.ad9361_transceiver_error_code());
        return;
    }

    const std::uint8_t original = tune_fb_clk ? fb : td;
    std::uint8_t value = original;

    auto apply = [&](std::uint8_t v) {
        std::uint8_t new_fb = tune_fb_clk ? v : fb;
        std::uint8_t new_td = tune_fb_clk ? td : v;
        if (!forge.set_ad9361_tx_clock_data_delay(new_fb, new_td)) {
            std::printf("error: write failed (%d)\n", forge.ad9361_transceiver_error_code());
            return false;
        }
        fb = new_fb;
        td = new_td;
        return true;
    };

    std::printf("\ntuning %s (0-15). [+] up  [-] down  [s] save & back  [q] cancel & back\n", field_name);
    std::printf("%s=%u\n", field_name, value);

    for (;;) {
        int c = read_key();
        if (c < 0) {
            break;
        }
        if ((c == '+' || c == '=') && value < 15) {
            std::uint8_t next = static_cast<std::uint8_t>(value + 1);
            if (apply(next)) {
                value = next;
                std::printf("%s=%u\n", field_name, value);
            }
        } else if ((c == '-' || c == '_') && value > 0) {
            std::uint8_t next = static_cast<std::uint8_t>(value - 1);
            if (apply(next)) {
                value = next;
                std::printf("%s=%u\n", field_name, value);
            }
        } else if (c == 's' || c == 'S') {
            std::printf("saved: %s=%u\n", field_name, value);
            return;
        } else if (c == 'q' || c == 'Q' || c == 27) {
            apply(original);
            std::printf("cancelled: %s reverted to %u\n", field_name, original);
            return;
        }
    }
}

// Live bit-toggle tuning of one LVDS invert control register (ctrl1 =
// REG_LVDS_INVERT_CTRL1, TX_FRAME/TX_D[5:0]; ctrl2 = REG_LVDS_INVERT_CTRL2,
// RX-side/clock bits). Each digit key 0-7 toggles that bit and writes
// immediately. 's' keeps the current value; 'q'/Esc reverts to whatever the
// register held on entry.
static void tune_lvds_invert_field(project::iq_forge &forge, const char *field_name, bool tune_ctrl1) {
    std::uint8_t c1 = 0, c2 = 0;
    if (!forge.get_ad9361_lvds_invert(c1, c2)) {
        std::printf("error: get lvds invert failed (%d)\n", forge.ad9361_transceiver_error_code());
        return;
    }

    const std::uint8_t original = tune_ctrl1 ? c1 : c2;
    std::uint8_t value = original;

    auto apply = [&](std::uint8_t v) {
        std::uint8_t new_c1 = tune_ctrl1 ? v : c1;
        std::uint8_t new_c2 = tune_ctrl1 ? c2 : v;
        if (!forge.set_ad9361_lvds_invert(new_c1, new_c2)) {
            std::printf("error: write failed (%d)\n", forge.ad9361_transceiver_error_code());
            return false;
        }
        c1 = new_c1;
        c2 = new_c2;
        return true;
    };

    std::printf("\ntuning %s (8 bits). [0-7] toggle bit  [s] save & back  [q] cancel & back\n", field_name);
    std::printf("%s=0x%02X\n", field_name, value);

    for (;;) {
        int c = read_key();
        if (c < 0) {
            break;
        }
        if (c >= '0' && c <= '7') {
            std::uint8_t bit = static_cast<std::uint8_t>(1u << (c - '0'));
            std::uint8_t next = static_cast<std::uint8_t>(value ^ bit);
            if (apply(next)) {
                value = next;
                std::printf("%s=0x%02X\n", field_name, value);
            }
        } else if (c == 's' || c == 'S') {
            std::printf("saved: %s=0x%02X\n", field_name, value);
            return;
        } else if (c == 'q' || c == 'Q' || c == 27) {
            apply(original);
            std::printf("cancelled: %s reverted to 0x%02X\n", field_name, original);
            return;
        }
    }
}

static void run_lvds_invert_tuner(project::iq_forge &forge) {
    if (!enable_raw_mode()) {
        std::printf("error: interactive tuner needs a real tty (run over 'ssh -t ...', not a piped/non-interactive session)\n");
        return;
    }

    for (;;) {
        std::uint8_t c1 = 0, c2 = 0;
        bool have = forge.get_ad9361_lvds_invert(c1, c2);
        std::printf("\nLVDS invert tuner");
        if (have) {
            std::printf(" (current: ctrl1=0x%02X ctrl2=0x%02X)", c1, c2);
        }
        std::printf("\n [1] tune ctrl1 (TX_FRAME/TX_D[5:0])   [2] tune ctrl2 (RX-side/clock)   [q] back\n");

        int c = read_key();
        if (c < 0 || c == 'q' || c == 'Q' || c == 27 || c == '0') {
            break;
        }
        if (c == '1') {
            tune_lvds_invert_field(forge, "ctrl1", true);
        } else if (c == '2') {
            tune_lvds_invert_field(forge, "ctrl2", false);
        }
    }

    disable_raw_mode();
}

static void run_tx_clock_data_delay_tuner(project::iq_forge &forge) {
    if (!enable_raw_mode()) {
        std::printf("error: interactive tuner needs a real tty (run over 'ssh -t ...', not a piped/non-interactive session)\n");
        return;
    }

    for (;;) {
        std::uint8_t fb = 0, td = 0;
        bool have = forge.get_ad9361_tx_clock_data_delay(fb, td);
        std::printf("\nTX clock/data delay tuner");
        if (have) {
            std::printf(" (current: fb_clk_delay=%u tx_data_delay=%u)", fb, td);
        }
        std::printf("\n [1] tune FB_CLK_DELAY   [2] tune TX_DATA_DELAY   [q] back\n");

        int c = read_key();
        if (c < 0 || c == 'q' || c == 'Q' || c == 27 || c == '0') {
            break;
        }
        if (c == '1') {
            tune_tx_clock_data_delay_field(forge, "fb_clk_delay", true);
        } else if (c == '2') {
            tune_tx_clock_data_delay_field(forge, "tx_data_delay", false);
        }
    }

    disable_raw_mode();
}

static void print_menu() {
    std::printf(
        "\n"
        "==== iq_forge console ====\n"
        " 1) TX LO frequency - read\n"
        " 2) TX LO frequency - set\n"
        " 3) TX attenuation - read\n"
        " 4) TX attenuation - set\n"
        " 5) RX AGC mode - set\n"
        " 6) TX enable/disable\n"
        " 7) TX state - read (ENSM)\n"
        " 8) DDS enable/disable\n"
        " 9) DDS state - read\n"
        "10) DDS frequency - read (phase increment / FTW)\n"
        "11) DDS frequency - set (phase increment / FTW)\n"
        "12) DDS reset\n"
        "13) TX clock/data delay - read (FB_CLK_DELAY / TX_DATA_DELAY)\n"
        "14) TX clock/data delay - set (FB_CLK_DELAY / TX_DATA_DELAY)\n"
        "15) TX clock/data delay - interactive tune (+/-, live)\n"
        "16) TX quadrature/LO-leakage - recalibrate\n"
        "17) LVDS invert - read (ctrl1/ctrl2)\n"
        "18) LVDS invert - set (ctrl1/ctrl2, raw byte 0-255)\n"
        "19) LVDS invert - interactive tune (bit toggle, live)\n"
        "20) AD9361 internal BIST tone - enable (TX1, bypasses LVDS/DDS)\n"
        "21) AD9361 internal BIST PRBS - enable (TX1, bypasses LVDS/DDS)\n"
        "22) AD9361 internal BIST - disable (back to normal digital data)\n"
        "23) AD9361 TX->RX digital loopback - enable (verify TX_D/TX_FRAME\n"
        "    data actually reaches the chip, via ad9361_rx_lvds_wrapper)\n"
        "24) AD9361 TX->RX digital loopback - disable\n"
        " 0) exit\n");
}

static void run_menu(project::iq_forge &forge) {
    for (;;) {
        print_menu();
        auto choice = read_choice("> ", 0, 24);
        if (!choice || *choice == 0) {
            return;
        }

        switch (*choice) {
            case 1: {
                std::uint64_t hz = 0;
                if (forge.get_ad9361_tx_lo_frequency(hz)) {
                    std::printf("tx-lo: %llu Hz\n", static_cast<unsigned long long>(hz));
                } else {
                    std::printf("error: get tx frequency failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 2: {
                auto hz = read_u64("Enter TX LO frequency in Hz: ");
                if (!hz) {
                    std::printf("invalid or cancelled\n");
                    break;
                }
                if (forge.set_ad9361_tx_lo_frequency(*hz)) {
                    std::printf("tx-lo: %llu Hz\n", static_cast<unsigned long long>(*hz));
                } else {
                    std::printf("error: set tx frequency failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 3: {
                std::uint32_t mdb = 0;
                if (forge.get_ad9361_tx_attenuation(mdb)) {
                    std::printf("tx-atten: %.3f dB\n", mdb / 1000.0);
                } else {
                    std::printf("error: get tx attenuation failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 4: {
                auto db = read_double("Enter TX attenuation in dB: ");
                if (!db || *db < 0.0) {
                    std::printf("invalid or cancelled\n");
                    break;
                }
                std::uint32_t mdb = static_cast<std::uint32_t>(*db * 1000.0 + 0.5);
                if (forge.set_ad9361_tx_attenuation(mdb)) {
                    std::printf("tx-atten: %.3f dB\n", mdb / 1000.0);
                } else {
                    std::printf("error: set tx attenuation failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 5: {
                auto mode = select_agc_mode();
                if (!mode) {
                    break;
                }
                if (!forge.set_ad9361_rx_gain_control_mode(*mode)) {
                    std::printf("error: set agc mode failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 6: {
                auto on = select_on_off();
                if (!on) {
                    break;
                }
                bool ok = *on ? forge.enable_ad9361_tx() : forge.disable_ad9361_tx();
                if (ok) {
                    std::printf("tx: %s\n", *on ? "enabled" : "disabled");
                } else {
                    std::printf("error: %s tx failed (%d)\n", *on ? "enable" : "disable",
                                forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 7: {
                drivers::ensm_state state;
                if (forge.get_ad9361_ensm_state(state)) {
                    std::printf("tx: %s\n", ensm_state_name(state));
                } else {
                    std::printf("error: get ensm state failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 8: {
                auto on = select_on_off();
                if (!on) {
                    break;
                }
                if (forge.set_dds_enabled(*on)) {
                    std::printf("dds: %s\n", *on ? "enabled" : "disabled");
                } else {
                    std::printf("error: %s\n", forge.dds_ctrl_gpio_error().c_str());
                }
                break;
            }
            case 9: {
                auto enabled = forge.dds_enabled();
                if (enabled) {
                    std::printf("dds: %s\n", *enabled ? "enabled" : "disabled");
                } else {
                    std::printf("error: %s\n", forge.dds_ctrl_gpio_error().c_str());
                }
                break;
            }
            case 10: {
                auto hz = forge.get_dds_frequency_hz();
                if (hz) {
                    std::printf("dds-freq: %.3f Hz\n", *hz);
                } else {
                    std::printf("error: %s\n", forge.dds_ftw_gpio_error().c_str());
                }
                break;
            }
            case 11: {
                auto hz = read_double("Enter DDS frequency in Hz: ");
                if (!hz || *hz < 0.0) {
                    std::printf("invalid or cancelled\n");
                    break;
                }
                if (forge.set_dds_frequency_hz(*hz)) {
                    auto actual = forge.get_dds_frequency_hz();
                    if (actual) {
                        std::printf("dds-freq: %.3f Hz (requested %.3f Hz, rounded to nearest FTW step)\n", *actual,
                                    *hz);
                    } else {
                        std::printf("dds-freq: set (requested %.3f Hz)\n", *hz);
                    }
                } else {
                    std::printf("error: %s\n", forge.dds_ftw_gpio_error().c_str());
                }
                break;
            }
            case 12: {
                if (forge.reset_dds()) {
                    std::printf("dds: reset\n");
                } else {
                    std::printf("error: %s\n", forge.dds_ctrl_gpio_error().c_str());
                }
                break;
            }
            case 13: {
                std::uint8_t fb_clk_delay = 0, tx_data_delay = 0;
                if (forge.get_ad9361_tx_clock_data_delay(fb_clk_delay, tx_data_delay)) {
                    std::printf("tx-clock-data-delay: fb_clk_delay=%u tx_data_delay=%u\n", fb_clk_delay,
                                tx_data_delay);
                } else {
                    std::printf("error: get tx clock/data delay failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 14: {
                auto fb_clk_delay = read_choice("Enter FB_CLK_DELAY (0-15): ", 0, 15);
                if (!fb_clk_delay) {
                    std::printf("invalid or cancelled\n");
                    break;
                }
                auto tx_data_delay = read_choice("Enter TX_DATA_DELAY (0-15): ", 0, 15);
                if (!tx_data_delay) {
                    std::printf("invalid or cancelled\n");
                    break;
                }
                if (forge.set_ad9361_tx_clock_data_delay(static_cast<std::uint8_t>(*fb_clk_delay),
                                                          static_cast<std::uint8_t>(*tx_data_delay))) {
                    std::printf("tx-clock-data-delay: fb_clk_delay=%ld tx_data_delay=%ld\n", *fb_clk_delay,
                                *tx_data_delay);
                } else {
                    std::printf("error: set tx clock/data delay failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 15: {
                run_tx_clock_data_delay_tuner(forge);
                break;
            }
            case 16: {
                if (forge.calibrate_ad9361_tx_quadrature()) {
                    std::printf("tx-quad-cal: done\n");
                } else {
                    std::printf("error: tx quad cal failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 17: {
                std::uint8_t c1 = 0, c2 = 0;
                if (forge.get_ad9361_lvds_invert(c1, c2)) {
                    std::printf("lvds-invert: ctrl1=0x%02X ctrl2=0x%02X\n", c1, c2);
                } else {
                    std::printf("error: get lvds invert failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 18: {
                auto c1 = read_choice("Enter ctrl1 (0-255): ", 0, 255);
                if (!c1) {
                    std::printf("invalid or cancelled\n");
                    break;
                }
                auto c2 = read_choice("Enter ctrl2 (0-255): ", 0, 255);
                if (!c2) {
                    std::printf("invalid or cancelled\n");
                    break;
                }
                if (forge.set_ad9361_lvds_invert(static_cast<std::uint8_t>(*c1), static_cast<std::uint8_t>(*c2))) {
                    std::printf("lvds-invert: ctrl1=0x%02lX ctrl2=0x%02lX\n", *c1, *c2);
                } else {
                    std::printf("error: set lvds invert failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 19: {
                run_lvds_invert_tuner(forge);
                break;
            }
            case 20: {
                auto hz = read_u64("Enter BIST tone frequency in Hz (0 for DC): ");
                if (!hz) {
                    std::printf("invalid or cancelled\n");
                    break;
                }
                // mask=0: BIST_MASK_CHANNEL_x bits *exclude* a channel from
                // injection, not include it - 0 masks nothing, so the tone
                // goes out on all channels (only TX1 is wired up here
                // anyway). level_db=0: full scale.
                if (forge.set_ad9361_bist_tone(drivers::bist_mode::inject_tx, static_cast<std::uint32_t>(*hz), 0,
                                                0x0)) {
                    std::printf("bist-tone: enabled at %llu Hz on TX1 (item 22 to disable)\n",
                                static_cast<unsigned long long>(*hz));
                } else {
                    std::printf("error: bist tone failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 21: {
                if (forge.set_ad9361_bist_prbs(drivers::bist_mode::inject_tx)) {
                    std::printf("bist-prbs: enabled on TX1 (item 22 to disable)\n");
                } else {
                    std::printf("error: bist prbs failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 22: {
                if (forge.set_ad9361_bist_prbs(drivers::bist_mode::disable)) {
                    std::printf("bist: disabled\n");
                } else {
                    std::printf("error: bist disable failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 23: {
                if (forge.set_ad9361_bist_loopback(1)) {
                    std::printf("loopback: enabled (TX digital data now mirrored to RX digital port)\n");
                } else {
                    std::printf("error: loopback enable failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            case 24: {
                if (forge.set_ad9361_bist_loopback(0)) {
                    std::printf("loopback: disabled\n");
                } else {
                    std::printf("error: loopback disable failed (%d)\n", forge.ad9361_transceiver_error_code());
                }
                break;
            }
            default:
                break;
        }
    }
}

int main(int argc, char **argv) {
    if (argc >= 2 &&
        (std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "help") == 0)) {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }

    auto manifest = read_manifest("manifest.env");
    if (!manifest) {
        std::fprintf(stderr, "error: cannot open manifest.env\n");
        return EXIT_FAILURE;
    }

    const std::string *bitstream = manifest_get(*manifest, "BITSTREAM");
    const std::string *dtbo = manifest_get(*manifest, "DTBO");
    const std::string *overlay_name = manifest_get(*manifest, "OVERLAY_NAME");
    if (!bitstream || !dtbo || !overlay_name) {
        std::fprintf(stderr, "error: manifest.env: missing BITSTREAM, DTBO, or OVERLAY_NAME\n");
        return EXIT_FAILURE;
    }

    hal::spi_config spi_cfg = hal::load_spi_config("spi.json").value_or(hal::spi_config{});

    std::optional<std::uintptr_t> ad9361_ctrl_gpio_base;
    if (auto it = manifest->find("AD9361_CTRL_GPIO_BASE"); it != manifest->end()) {
        ad9361_ctrl_gpio_base = static_cast<std::uintptr_t>(std::strtoull(it->second.c_str(), nullptr, 0));
    }
    std::optional<std::uintptr_t> dds_ctrl_gpio_base;
    if (auto it = manifest->find("DDS_CTRL_GPIO_BASE"); it != manifest->end()) {
        dds_ctrl_gpio_base = static_cast<std::uintptr_t>(std::strtoull(it->second.c_str(), nullptr, 0));
    }
    std::optional<std::uintptr_t> dds_ftw_gpio_base;
    if (auto it = manifest->find("DDS_FTW_GPIO_BASE"); it != manifest->end()) {
        dds_ftw_gpio_base = static_cast<std::uintptr_t>(std::strtoull(it->second.c_str(), nullptr, 0));
    }
    std::optional<double> dds_clk_hz;
    if (auto it = manifest->find("DDS_CLK_HZ"); it != manifest->end()) {
        dds_clk_hz = std::strtod(it->second.c_str(), nullptr);
    }

    project::iq_forge forge(spi_cfg, ad9361_ctrl_gpio_base, dds_ctrl_gpio_base, dds_ftw_gpio_base, dds_clk_hz);

    if (forge.fpga_state() == "operating") {
        std::printf("fpga already operating, skip reload\n");
    } else {
        auto load_result = forge.load_fpga_bitstream(*bitstream);
        if (!load_result) {
            std::fprintf(stderr, "error: %s\n", load_result.message.c_str());
            return EXIT_FAILURE;
        }
        std::printf("fpga loaded: state=%s\n", load_result.state.c_str());
    }

    if (forge.overlay_status(*overlay_name) == "applied") {
        std::printf("overlay '%s' already applied, skip\n", overlay_name->c_str());
    } else {
        if (!forge.apply_fpga_overlay(*overlay_name, *dtbo, true)) {
            std::fprintf(stderr, "error: failed to apply overlay '%s'\n", overlay_name->c_str());
            return EXIT_FAILURE;
        }
        std::printf("overlay '%s' applied\n", overlay_name->c_str());
    }

    if (!forge.bring_up_ad9361()) {
        std::fprintf(stderr, "error: %s\n", forge.ad9361_ctrl_gpio_error().c_str());
        return EXIT_FAILURE;
    }

    auto vendor_id = forge.read_ad9361_vendor_id();
    if (!vendor_id) {
        std::fprintf(stderr, "error: %s\n", forge.ad9361_spi_error().c_str());
        return EXIT_FAILURE;
    }
    std::printf("vendor-id: 0x%02x\n", *vendor_id);

    if (!forge.init_ad9361_transceiver()) {
        std::fprintf(stderr, "error: transceiver init failed (%d)\n", forge.ad9361_transceiver_error_code());
        return EXIT_FAILURE;
    }
    std::printf("ad9361: transceiver initialized\n");

    run_menu(forge);
    return EXIT_SUCCESS;
}
