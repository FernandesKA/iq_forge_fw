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

        private:
            void *m_map_base;
            std::size_t m_map_size;
            volatile std::uint32_t *m_reg;
    };

}
