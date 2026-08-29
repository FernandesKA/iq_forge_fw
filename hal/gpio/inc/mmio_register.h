/**
 * @file mmio_register.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-27
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace hal {

    // Single 32-bit register at a physical address, mapped via /dev/mem.
    class mmio_register {
        public:
            explicit mmio_register(std::uintptr_t physical_address);
            ~mmio_register();

            mmio_register(const mmio_register &) = delete;
            mmio_register &operator=(const mmio_register &) = delete;

            std::uint32_t read() const;
            void write(std::uint32_t value) const;

            bool ok() const;
            const std::string &last_error() const;

        private:
            bool ensure_mapped() const;

            std::uintptr_t m_physical_address;
            mutable void *m_map_base;
            mutable std::size_t m_map_size;
            mutable volatile std::uint32_t *m_reg;
            mutable std::string m_last_error;
    };

}
