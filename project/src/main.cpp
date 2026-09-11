/**
 * @file main.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief CLI entry point for project::iq_forge: AD9361 vendor id readback,
 *        FPGA bitstream load, device-tree overlay apply. Meant to be
 *        deployed to the target board (see scripts/deploy.sh, scripts/load.sh).
 *        Run with no arguments to do the whole thing using manifest.env +
 *        spi.json from the current directory - that's what a deployed
 *        archive extracts to, so no flags are needed on target.
 *        Run with any argument (e.g. "menu") to open an interactive,
 *        number-selected menu for manual use instead.
 * @version 0.1
 * @date 2026-08-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "iq_forge.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>

static void usage(const char *prog) {
    std::fprintf(stderr,
        "Usage: %s        Reads manifest.env + spi.json from the current directory,\n"
        "                loads the bitstream, applies the overlay, reads vendor id.\n"
        "                This is what a deployed archive runs unattended.\n"
        "       %s menu   Opens an interactive, number-selected menu for manual use\n"
        "                (vendor id, tx settings, dds on/off, fpga load, overlay apply).\n"
        "                Also reads manifest.env + spi.json from the current directory.\n",
        prog, prog);
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

// Prints a prompt, reads one line from stdin. Empty string on EOF.
static std::string prompt_line(const char *message) {
    std::printf("%s", message);
    std::fflush(stdout);
    std::string line;
    if (!std::getline(std::cin, line)) {
        return {};
    }
    return line;
}

static void print_menu_help() {
    std::printf(
        " 1) status         show current AD9361/DDS state, read live from the hardware\n"
        " 2) init           bring up AD9361 (reset + SPI) and init the transceiver\n"
        " 3) vendor-id      read the AD9361 vendor id over SPI\n"
        " 4) tx frequency   set TX LO frequency in Hz, or read it back\n"
        " 5) tx attenuation set TX attenuation in dB, or read it back\n"
        " 6) agc mode       set RX gain control mode\n"
        " 7) tx on/off      enable/disable the AD9361 TX path (ENSM), or read it back\n"
        " 8) dds on/off     enable/disable the DDS sine output, or read it back\n"
        " 9) load fpga      load an FPGA bitstream\n"
        "10) apply overlay  apply a device-tree overlay\n"
        "11) help           show this text\n"
        " 0) quit\n");
}

static void menu_status(project::iq_forge &forge, bool initialized, std::optional<bool> dds_on,
                         std::optional<std::uintptr_t> dds_gpio_base) {
    std::printf("ad9361: %s\n", initialized ? "initialized" : "not initialized");
    if (initialized) {
        std::uint64_t hz = 0;
        if (forge.get_ad9361_tx_lo_frequency(hz)) {
            std::printf("tx-lo: %llu Hz\n", static_cast<unsigned long long>(hz));
        } else {
            std::printf("tx-lo: error (%d)\n", forge.ad9361_transceiver_error_code());
        }

        std::uint32_t mdb = 0;
        if (forge.get_ad9361_tx_attenuation(mdb)) {
            std::printf("tx-atten: %.3f dB\n", mdb / 1000.0);
        } else {
            std::printf("tx-atten: error (%d)\n", forge.ad9361_transceiver_error_code());
        }

        drivers::ensm_state state;
        if (forge.get_ad9361_ensm_state(state)) {
            std::printf("tx: %s\n", ensm_state_name(state));
        } else {
            std::printf("tx: error (%d)\n", forge.ad9361_transceiver_error_code());
        }
    }
    std::printf("dds gpio base: ");
    if (dds_gpio_base) {
        std::printf("0x%llx\n", static_cast<unsigned long long>(*dds_gpio_base));
        auto enabled = forge.dds_enabled();
        std::printf("dds: %s\n", enabled ? (*enabled ? "enabled" : "disabled") : "error");
    } else {
        std::printf("not set\n");
        std::printf("dds: %s\n", dds_on ? (*dds_on ? "enabled" : "disabled") : "unknown (not touched this session)");
    }
}

// Interactive, number-selected session: keeps one project::iq_forge instance
// alive for the whole session so 'init', 'tx frequency', 'agc mode' and
// 'dds on/off' can be issued one at a time instead of passing them all as
// argv on every invocation. Picks up spi.json / manifest.env from the
// current directory - no command-line flags needed.
static int cmd_menu() {
    std::string config_path = "spi.json";
    std::optional<std::uintptr_t> ctrl_gpio_base;
    std::optional<std::uintptr_t> dds_gpio_base;

    if (auto manifest = read_manifest("manifest.env")) {
        if (auto it = manifest->find("AD9361_CTRL_GPIO_BASE"); it != manifest->end()) {
            ctrl_gpio_base = static_cast<std::uintptr_t>(std::strtoull(it->second.c_str(), nullptr, 0));
        }
        if (auto it = manifest->find("DDS_CTRL_GPIO_BASE"); it != manifest->end()) {
            dds_gpio_base = static_cast<std::uintptr_t>(std::strtoull(it->second.c_str(), nullptr, 0));
        }
    }

    hal::spi_config cfg = hal::load_spi_config(config_path).value_or(hal::spi_config{});
    project::iq_forge forge(cfg, ctrl_gpio_base, dds_gpio_base);

    bool initialized = false;
    std::optional<bool> dds_on;

    std::printf("iq_forge interactive menu. Type '11' for help, '0' to quit.\n");
    if (!dds_gpio_base) {
        std::printf("note: no DDS control GPIO base (DDS_CTRL_GPIO_BASE in manifest.env) --\n"
                     "      'dds on/off' will error until one is set\n");
    }
    print_menu_help();

    while (true) {
        std::string sel = prompt_line("> ");
        if (sel.empty() && std::cin.eof()) {
            std::printf("\n");
            break;
        }
        if (sel.empty()) {
            continue;
        }

        char *end = nullptr;
        long choice = std::strtol(sel.c_str(), &end, 10);
        if (end != sel.c_str() + sel.size()) {
            std::printf("unknown selection '%s' (type 11 for help)\n", sel.c_str());
            continue;
        }

        if (choice == 0) {
            break;
        }

        if (choice == 1) {
            menu_status(forge, initialized, dds_on, dds_gpio_base);
            continue;
        }

        if (choice == 2) {
            if (!forge.bring_up_ad9361()) {
                std::printf("error: %s\n", forge.ad9361_ctrl_gpio_error().c_str());
                continue;
            }

            auto vendor_id = forge.read_ad9361_vendor_id();
            if (!vendor_id) {
                std::printf("error: %s\n", forge.ad9361_spi_error().c_str());
                continue;
            }
            std::printf("vendor-id: 0x%02x\n", *vendor_id);

            if (!forge.init_ad9361_transceiver()) {
                std::printf("error: transceiver init failed (%d)\n", forge.ad9361_transceiver_error_code());
                continue;
            }
            initialized = true;
            std::printf("ad9361: transceiver initialized\n");
            continue;
        }

        if (choice == 3) {
            auto vendor_id = forge.read_ad9361_vendor_id();
            if (!vendor_id) {
                std::printf("error: %s\n", forge.ad9361_spi_error().c_str());
                continue;
            }
            std::printf("vendor-id: 0x%02x\n", *vendor_id);
            continue;
        }

        if (choice == 4) {
            if (!initialized) {
                std::printf("error: run '2' (init) first\n");
                continue;
            }
            std::string hz_str = prompt_line("TX frequency in Hz (blank = read current): ");
            if (hz_str.empty()) {
                std::uint64_t hz = 0;
                if (!forge.get_ad9361_tx_lo_frequency(hz)) {
                    std::printf("error: get tx frequency failed (%d)\n", forge.ad9361_transceiver_error_code());
                    continue;
                }
                std::printf("tx-lo: %llu Hz\n", static_cast<unsigned long long>(hz));
                continue;
            }
            auto hz = parse_u64(hz_str);
            if (!hz) {
                std::printf("error: invalid frequency '%s'\n", hz_str.c_str());
                continue;
            }
            if (!forge.set_ad9361_tx_lo_frequency(*hz)) {
                std::printf("error: set tx frequency failed (%d)\n", forge.ad9361_transceiver_error_code());
                continue;
            }
            std::printf("tx-lo: %llu Hz\n", static_cast<unsigned long long>(*hz));
            continue;
        }

        if (choice == 5) {
            if (!initialized) {
                std::printf("error: run '2' (init) first\n");
                continue;
            }
            std::string db_str = prompt_line("TX attenuation in dB (blank = read current): ");
            if (db_str.empty()) {
                std::uint32_t mdb = 0;
                if (!forge.get_ad9361_tx_attenuation(mdb)) {
                    std::printf("error: get tx attenuation failed (%d)\n", forge.ad9361_transceiver_error_code());
                    continue;
                }
                std::printf("tx-atten: %.3f dB\n", mdb / 1000.0);
                continue;
            }
            char *db_end = nullptr;
            double db = std::strtod(db_str.c_str(), &db_end);
            if (db_end != db_str.c_str() + db_str.size() || db < 0.0) {
                std::printf("error: invalid attenuation '%s'\n", db_str.c_str());
                continue;
            }
            std::uint32_t mdb = static_cast<std::uint32_t>(db * 1000.0 + 0.5);
            if (!forge.set_ad9361_tx_attenuation(mdb)) {
                std::printf("error: set tx attenuation failed (%d)\n", forge.ad9361_transceiver_error_code());
                continue;
            }
            std::printf("tx-atten: %.3f dB\n", mdb / 1000.0);
            continue;
        }

        if (choice == 6) {
            std::string mode_str = prompt_line("AGC mode - 1) manual 2) fast 3) slow 4) hybrid: ");
            drivers::rx_gain_mode mode;
            const char *mode_name = nullptr;
            if (mode_str == "1") {
                mode = drivers::rx_gain_mode::manual;
                mode_name = "manual";
            } else if (mode_str == "2") {
                mode = drivers::rx_gain_mode::fast_attack_agc;
                mode_name = "fast";
            } else if (mode_str == "3") {
                mode = drivers::rx_gain_mode::slow_attack_agc;
                mode_name = "slow";
            } else if (mode_str == "4") {
                mode = drivers::rx_gain_mode::hybrid_agc;
                mode_name = "hybrid";
            } else {
                std::printf("error: unknown selection '%s'\n", mode_str.c_str());
                continue;
            }
            if (!initialized) {
                std::printf("error: run '2' (init) first\n");
                continue;
            }
            if (!forge.set_ad9361_rx_gain_control_mode(mode)) {
                std::printf("error: set agc mode failed (%d)\n", forge.ad9361_transceiver_error_code());
                continue;
            }
            std::printf("agc: %s\n", mode_name);
            continue;
        }

        if (choice == 7) {
            if (!initialized) {
                std::printf("error: run '2' (init) first\n");
                continue;
            }
            std::string sub = prompt_line("tx - 1) on 2) off (blank = read current state): ");
            if (sub.empty()) {
                drivers::ensm_state state;
                if (!forge.get_ad9361_ensm_state(state)) {
                    std::printf("error: get ensm state failed (%d)\n", forge.ad9361_transceiver_error_code());
                    continue;
                }
                std::printf("tx: %s\n", ensm_state_name(state));
                continue;
            }
            if (sub != "1" && sub != "2") {
                std::printf("error: unknown selection '%s'\n", sub.c_str());
                continue;
            }
            bool enable = sub == "1";
            bool ok = enable ? forge.enable_ad9361_tx() : forge.disable_ad9361_tx();
            if (!ok) {
                std::printf("error: %s tx failed (%d)\n", enable ? "enable" : "disable",
                            forge.ad9361_transceiver_error_code());
                continue;
            }
            std::printf("tx: %s\n", enable ? "enabled" : "disabled");
            continue;
        }

        if (choice == 8) {
            if (!dds_gpio_base) {
                std::printf("error: no DDS control GPIO base (set DDS_CTRL_GPIO_BASE in manifest.env)\n");
                continue;
            }
            std::string sub = prompt_line("dds - 1) on 2) off (blank = read current state): ");
            if (sub.empty()) {
                auto enabled = forge.dds_enabled();
                if (!enabled) {
                    std::printf("error: %s\n", forge.dds_ctrl_gpio_error().c_str());
                    continue;
                }
                std::printf("dds: %s\n", *enabled ? "enabled" : "disabled");
                continue;
            }
            if (sub != "1" && sub != "2") {
                std::printf("error: unknown selection '%s'\n", sub.c_str());
                continue;
            }
            bool enable = sub == "1";
            if (!forge.set_dds_enabled(enable)) {
                std::printf("error: %s\n", forge.dds_ctrl_gpio_error().c_str());
                continue;
            }
            dds_on = enable;
            std::printf("dds: %s\n", enable ? "enabled" : "disabled");
            continue;
        }

        if (choice == 9) {
            std::string path = prompt_line("Bitstream path (blank = cancel): ");
            if (path.empty()) {
                continue;
            }
            auto result = forge.load_fpga_bitstream(path);
            if (!result) {
                std::printf("error: %s\n", result.message.c_str());
                continue;
            }
            std::printf("fpga loaded: state=%s\n", result.state.c_str());
            continue;
        }

        if (choice == 10) {
            std::string name = prompt_line("Overlay name (blank = cancel): ");
            if (name.empty()) {
                continue;
            }
            std::string dtbo_path = prompt_line("Overlay .dtbo path: ");
            if (dtbo_path.empty()) {
                std::printf("error: no .dtbo path given\n");
                continue;
            }
            std::string replace_str = prompt_line("Replace if already applied? 1) yes 2) no [2]: ");
            bool replace = replace_str == "1";
            if (!forge.apply_fpga_overlay(name, dtbo_path, replace)) {
                std::printf("error: failed to apply overlay '%s'\n", name.c_str());
                continue;
            }
            std::printf("overlay '%s' applied\n", name.c_str());
            continue;
        }

        if (choice == 11) {
            print_menu_help();
            continue;
        }

        std::printf("unknown selection '%s' (type 11 for help)\n", sel.c_str());
    }

    return EXIT_SUCCESS;
}

static int cmd_start() {
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

    project::iq_forge forge(spi_cfg, ad9361_ctrl_gpio_base);

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

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return cmd_start();
    }

    if (std::string(argv[1]) == "menu") {
        return cmd_menu();
    }

    usage(argv[0]);
    return EXIT_FAILURE;
}
