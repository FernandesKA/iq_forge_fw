#include "dds_lfm_gpio.h"

namespace drivers {

    dds_lfm_gpio::dds_lfm_gpio(std::uintptr_t mmio_base)
        : m_start(mmio_base), m_stop(mmio_base + kRegisterStride), m_incr(mmio_base + 2 * kRegisterStride) {}

    bool dds_lfm_gpio::write(const hal::mmio_register &reg, std::uint32_t ftw) const {
        reg.write(ftw & kFtwMask);
        m_last_error = reg.ok() ? std::string() : reg.last_error();
        return reg.ok();
    }

    bool dds_lfm_gpio::read(const hal::mmio_register &reg, std::uint32_t &ftw) const {
        std::uint32_t v = reg.read();
        if (!reg.ok()) {
            m_last_error = reg.last_error();
            return false;
        }
        m_last_error.clear();
        ftw = v & kFtwMask;
        return true;
    }

    bool dds_lfm_gpio::set_start(std::uint32_t ftw) const { return write(m_start, ftw); }
    bool dds_lfm_gpio::get_start(std::uint32_t &ftw) const { return read(m_start, ftw); }
    bool dds_lfm_gpio::set_stop(std::uint32_t ftw) const { return write(m_stop, ftw); }
    bool dds_lfm_gpio::get_stop(std::uint32_t &ftw) const { return read(m_stop, ftw); }
    bool dds_lfm_gpio::set_incr(std::uint32_t ftw_per_clk) const { return write(m_incr, ftw_per_clk); }
    bool dds_lfm_gpio::get_incr(std::uint32_t &ftw_per_clk) const { return read(m_incr, ftw_per_clk); }

}
