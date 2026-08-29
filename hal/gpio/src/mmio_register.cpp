/**
 * @file mmio_register.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-27
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "mmio_register.h"

#include <cstring>
#include <sstream>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace hal {

    mmio_register::mmio_register(std::uintptr_t physical_address)
        : m_physical_address(physical_address), m_map_base(nullptr), m_map_size(0), m_reg(nullptr) {
    }

    mmio_register::~mmio_register() {
        if (m_map_base != nullptr) {
            ::munmap(m_map_base, m_map_size);
        }
    }

    bool mmio_register::ensure_mapped() const {
        if (m_reg != nullptr) {
            return true;
        }

        int fd = ::open("/dev/mem", O_RDWR | O_SYNC);
        if (fd < 0) {
            m_last_error = std::string("mmio_register: failed to open /dev/mem: ") + std::strerror(errno);
            return false;
        }

        std::uintptr_t page_size = static_cast<std::uintptr_t>(::sysconf(_SC_PAGESIZE));
        std::uintptr_t aligned = m_physical_address & ~(page_size - 1);
        std::uintptr_t offset = m_physical_address - aligned;

        void *map = ::mmap(nullptr, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, static_cast<off_t>(aligned));
        ::close(fd);

        if (map == MAP_FAILED) {
            std::ostringstream ss;
            ss << "mmio_register: mmap failed for physical address 0x" << std::hex << m_physical_address
               << ": " << std::strerror(errno);
            m_last_error = ss.str();
            return false;
        }

        m_map_base = map;
        m_map_size = page_size;
        m_reg = reinterpret_cast<volatile std::uint32_t *>(reinterpret_cast<std::uint8_t *>(map) + offset);
        m_last_error.clear();
        return true;
    }

    std::uint32_t mmio_register::read() const {
        if (!ensure_mapped()) {
            return 0;
        }
        return *m_reg;
    }

    void mmio_register::write(std::uint32_t value) const {
        if (!ensure_mapped()) {
            return;
        }
        *m_reg = value;
    }

    bool mmio_register::ok() const {
        return m_reg != nullptr;
    }

    const std::string &mmio_register::last_error() const {
        return m_last_error;
    }

}
