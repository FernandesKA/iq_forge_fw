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
        " 0) exit\n");
}

static void run_menu(project::iq_forge &forge) {
    for (;;) {
        print_menu();
        auto choice = read_choice("> ", 0, 12);
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
