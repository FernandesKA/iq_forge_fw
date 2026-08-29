/**
 * @file main.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief CLI entry point for project::iq_forge: AD9361 vendor id readback,
 *        FPGA bitstream load, device-tree overlay apply. Meant to be
 *        deployed to the target board (see scripts/deploy.sh, scripts/load.sh).
 *        Run with no arguments (or "start") to do the whole thing using
 *        manifest.env + spi.json from the current directory - that's what
 *        a deployed archive extracts to, so no flags are needed on target.
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
#include <cstring>
#include <fstream>
#include <map>
#include <optional>

static void usage(const char *prog) {
    std::fprintf(stderr,
        "Usage: %s [start]\n"
        "         Reads manifest.env + spi.json from the current directory,\n"
        "         loads the bitstream, applies the overlay, reads vendor id.\n"
        "       %s vendor-id [--config <spi.json>] [--spi <path>]\n"
        "       %s load-fpga <bitstream.bin>\n"
        "       %s apply-overlay <name> <overlay.dtbo> [--replace]\n"
        "       %s tx --freq <hz> [--agc manual|fast|slow|hybrid]\n"
        "                [--config <spi.json>] [--ctrl-gpio-base <addr>]\n",
        prog, prog, prog, prog, prog);
}

static std::optional<drivers::rx_gain_mode> parse_agc_mode(const std::string &name) {
    if (name == "manual") {
        return drivers::rx_gain_mode::manual;
    }
    if (name == "fast") {
        return drivers::rx_gain_mode::fast_attack_agc;
    }
    if (name == "slow") {
        return drivers::rx_gain_mode::slow_attack_agc;
    }
    if (name == "hybrid") {
        return drivers::rx_gain_mode::hybrid_agc;
    }
    return std::nullopt;
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
    if (argc < 2 || std::strcmp(argv[1], "start") == 0) {
        return cmd_start();
    }

    const std::string cmd = argv[1];

    if (cmd == "vendor-id") {
        std::string config_path = "spi.json";
        std::string spi_override;
        bool config_flag_given = false;

        for (int i = 2; i < argc; ++i) {
            if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
                config_path = argv[++i];
                config_flag_given = true;
            } else if (std::strcmp(argv[i], "--spi") == 0 && i + 1 < argc) {
                spi_override = argv[++i];
            }
        }

        auto loaded_cfg = hal::load_spi_config(config_path);
        if (!loaded_cfg && config_flag_given) {
            std::fprintf(stderr, "error: cannot open or parse %s\n", config_path.c_str());
            return EXIT_FAILURE;
        }
        hal::spi_config cfg = loaded_cfg.value_or(hal::spi_config{});

        if (!spi_override.empty()) {
            cfg.device = spi_override;
        }

        project::iq_forge forge(cfg);
        auto vendor_id = forge.read_ad9361_vendor_id();
        if (!vendor_id) {
            std::fprintf(stderr, "error: %s\n", forge.ad9361_spi_error().c_str());
            return EXIT_FAILURE;
        }
        std::printf("0x%02x\n", *vendor_id);
        return EXIT_SUCCESS;
    }

    if (cmd == "tx") {
        std::string config_path = "spi.json";
        std::optional<std::uint64_t> freq_hz;
        std::optional<drivers::rx_gain_mode> agc_mode;
        std::optional<std::uintptr_t> ctrl_gpio_base;

        for (int i = 2; i < argc; ++i) {
            if (std::strcmp(argv[i], "--freq") == 0 && i + 1 < argc) {
                freq_hz = std::strtoull(argv[++i], nullptr, 0);
            } else if (std::strcmp(argv[i], "--agc") == 0 && i + 1 < argc) {
                agc_mode = parse_agc_mode(argv[++i]);
                if (!agc_mode) {
                    std::fprintf(stderr, "error: unknown --agc value '%s'\n", argv[i]);
                    return EXIT_FAILURE;
                }
            } else if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
                config_path = argv[++i];
            } else if (std::strcmp(argv[i], "--ctrl-gpio-base") == 0 && i + 1 < argc) {
                ctrl_gpio_base = static_cast<std::uintptr_t>(std::strtoull(argv[++i], nullptr, 0));
            }
        }

        if (!freq_hz) {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        hal::spi_config cfg = hal::load_spi_config(config_path).value_or(hal::spi_config{});

        project::iq_forge forge(cfg, ctrl_gpio_base);

        if (!forge.bring_up_ad9361()) {
            std::fprintf(stderr, "error: %s\n", forge.ad9361_ctrl_gpio_error().c_str());
            return EXIT_FAILURE;
        }

        if (!forge.init_ad9361_transceiver()) {
            std::fprintf(stderr, "error: transceiver init failed (%d)\n", forge.ad9361_transceiver_error_code());
            return EXIT_FAILURE;
        }

        if (!forge.set_ad9361_tx_lo_frequency(*freq_hz)) {
            std::fprintf(stderr, "error: set tx frequency failed (%d)\n", forge.ad9361_transceiver_error_code());
            return EXIT_FAILURE;
        }
        std::printf("tx-lo: %llu Hz\n", static_cast<unsigned long long>(*freq_hz));

        if (agc_mode) {
            if (!forge.set_ad9361_rx_gain_control_mode(*agc_mode)) {
                std::fprintf(stderr, "error: set agc mode failed (%d)\n", forge.ad9361_transceiver_error_code());
                return EXIT_FAILURE;
            }
            std::printf("agc: enabled\n");
        }

        if (!forge.enable_ad9361_tx()) {
            std::fprintf(stderr, "error: enable tx failed (%d)\n", forge.ad9361_transceiver_error_code());
            return EXIT_FAILURE;
        }
        std::printf("tx: enabled\n");

        return EXIT_SUCCESS;
    }

    if (cmd == "load-fpga") {
        if (argc < 3) {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        project::iq_forge forge;
        auto result = forge.load_fpga_bitstream(argv[2]);
        if (!result) {
            std::fprintf(stderr, "error: %s\n", result.message.c_str());
            return EXIT_FAILURE;
        }

        std::printf("fpga loaded: state=%s\n", result.state.c_str());
        return EXIT_SUCCESS;
    }

    if (cmd == "apply-overlay") {
        if (argc < 4) {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        bool replace = false;
        for (int i = 4; i < argc; ++i) {
            if (std::strcmp(argv[i], "--replace") == 0) {
                replace = true;
            }
        }

        project::iq_forge forge;
        if (!forge.apply_fpga_overlay(argv[2], argv[3], replace)) {
            std::fprintf(stderr, "error: failed to apply overlay '%s'\n", argv[2]);
            return EXIT_FAILURE;
        }

        std::printf("overlay '%s' applied\n", argv[2]);
        return EXIT_SUCCESS;
    }

    usage(argv[0]);
    return EXIT_FAILURE;
}
