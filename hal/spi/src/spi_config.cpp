/**
 * @file spi_config.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Minimal reader for the flat spi.json config (device/mode/bits_per_word/
 *        speed_hz only) - not a general-purpose JSON parser.
 * @version 0.1
 * @date 2026-08-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "spi_config.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace hal {

    namespace {

        std::string read_file(const std::string &path) {
            std::ifstream in(path, std::ios::binary);
            if (!in) {
                throw std::runtime_error("spi_config: cannot open " + path);
            }
            std::ostringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }

        bool find_value(const std::string &json, const std::string &key, std::string &out) {
            std::string needle = "\"" + key + "\"";
            std::size_t pos = json.find(needle);
            if (pos == std::string::npos) {
                return false;
            }

            pos = json.find(':', pos + needle.size());
            if (pos == std::string::npos) {
                throw std::runtime_error("spi_config: malformed entry for \"" + key + "\"");
            }
            ++pos;

            while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
                ++pos;
            }

            if (pos < json.size() && json[pos] == '"') {
                std::size_t end = json.find('"', pos + 1);
                if (end == std::string::npos) {
                    throw std::runtime_error("spi_config: unterminated string for \"" + key + "\"");
                }
                out = json.substr(pos + 1, end - pos - 1);
                return true;
            }

            std::size_t end = json.find_first_of(",}\n\r\t ", pos);
            if (end == std::string::npos) {
                end = json.size();
            }
            out = json.substr(pos, end - pos);
            return true;
        }

    }

    spi_config load_spi_config(const std::string &path) {
        std::string json = read_file(path);
        spi_config cfg;

        std::string value;
        if (find_value(json, "device", value)) {
            cfg.device = value;
        } else {
            throw std::runtime_error("spi_config: \"device\" is required in " + path);
        }

        if (find_value(json, "mode", value)) {
            cfg.mode = static_cast<std::uint8_t>(std::strtoul(value.c_str(), nullptr, 0));
        }
        if (find_value(json, "bits_per_word", value)) {
            cfg.bits_per_word = static_cast<std::uint8_t>(std::strtoul(value.c_str(), nullptr, 0));
        }
        if (find_value(json, "speed_hz", value)) {
            cfg.speed_hz = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 0));
        }

        return cfg;
    }

}
