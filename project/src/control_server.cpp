#include "control_server.h"

#include "iq_forge.h"

#include <cerrno>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace net {

namespace {

std::uint64_t double_to_bits(double d) {
    std::uint64_t u;
    std::memcpy(&u, &d, sizeof(u));
    return u;
}

double bits_to_double(std::uint64_t u) {
    double d;
    std::memcpy(&d, &u, sizeof(d));
    return d;
}

void encode(const packet &p, std::uint8_t out[kPacketSize]) {
    out[0] = static_cast<std::uint8_t>(p.magic >> 24);
    out[1] = static_cast<std::uint8_t>(p.magic >> 16);
    out[2] = static_cast<std::uint8_t>(p.magic >> 8);
    out[3] = static_cast<std::uint8_t>(p.magic);
    out[4] = static_cast<std::uint8_t>(p.command >> 8);
    out[5] = static_cast<std::uint8_t>(p.command);
    out[6] = static_cast<std::uint8_t>(p.code >> 8);
    out[7] = static_cast<std::uint8_t>(p.code);
    out[8] = static_cast<std::uint8_t>(p.query_id >> 24);
    out[9] = static_cast<std::uint8_t>(p.query_id >> 16);
    out[10] = static_cast<std::uint8_t>(p.query_id >> 8);
    out[11] = static_cast<std::uint8_t>(p.query_id);
    for (int i = 0; i < 8; ++i) {
        out[12 + i] = static_cast<std::uint8_t>(p.arg >> (56 - 8 * i));
    }
}

packet decode(const std::uint8_t in[kPacketSize]) {
    packet p;
    p.magic = (static_cast<std::uint32_t>(in[0]) << 24) | (static_cast<std::uint32_t>(in[1]) << 16) |
              (static_cast<std::uint32_t>(in[2]) << 8) | static_cast<std::uint32_t>(in[3]);
    p.command = static_cast<std::uint16_t>((in[4] << 8) | in[5]);
    p.code = static_cast<std::uint16_t>((in[6] << 8) | in[7]);
    p.query_id = (static_cast<std::uint32_t>(in[8]) << 24) | (static_cast<std::uint32_t>(in[9]) << 16) |
                 (static_cast<std::uint32_t>(in[10]) << 8) | static_cast<std::uint32_t>(in[11]);
    p.arg = 0;
    for (int i = 0; i < 8; ++i) {
        p.arg = (p.arg << 8) | in[12 + i];
    }
    return p;
}

} // namespace

control_server::control_server(project::iq_forge &forge, std::uint16_t port) : m_forge(forge), m_port(port) {}

control_server::~control_server() { stop(); }

bool control_server::start(std::string &error_out) {
    m_listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_listen_fd < 0) {
        error_out = std::string("socket() failed: ") + std::strerror(errno);
        return false;
    }

    int reuse = 1;
    ::setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);

    if (::bind(m_listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        error_out = std::string("bind() failed: ") + std::strerror(errno);
        ::close(m_listen_fd);
        m_listen_fd = -1;
        return false;
    }
    if (::listen(m_listen_fd, 1) != 0) {
        error_out = std::string("listen() failed: ") + std::strerror(errno);
        ::close(m_listen_fd);
        m_listen_fd = -1;
        return false;
    }

    m_running.store(true);
    m_thread = std::thread(&control_server::accept_loop, this);
    return true;
}

void control_server::stop() {
    if (!m_running.exchange(false)) {
        return;
    }
    if (m_listen_fd >= 0) {
        // shutdown() (not just close()) is what actually unblocks a thread
        // sitting in accept() on this socket.
        ::shutdown(m_listen_fd, SHUT_RDWR);
        ::close(m_listen_fd);
        m_listen_fd = -1;
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void control_server::accept_loop() {
    while (m_running.load()) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = ::accept(m_listen_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
        if (client_fd < 0) {
            // stop() closed the listening socket (m_running now false), or
            // a transient accept() error -- either way, loop condition
            // above decides whether to try again.
            continue;
        }
        int one = 1;
        ::setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        handle_client(client_fd);
        ::close(client_fd);
    }
}

void control_server::handle_client(int client_fd) {
    while (m_running.load()) {
        std::uint8_t raw[kPacketSize];
        std::size_t have = 0;
        while (have < kPacketSize) {
            ssize_t n = ::recv(client_fd, raw + have, kPacketSize - have, 0);
            if (n <= 0) {
                return; // client disconnected or socket error
            }
            have += static_cast<std::size_t>(n);
        }

        packet req = decode(raw);
        packet resp;
        resp.query_id = req.query_id;

        if (req.magic != kProtocolMagic) {
            // Framing is untrustworthy once the magic doesn't match (the
            // rest of this "packet" may not even be a real header) --
            // safer to drop the connection than to guess where the next
            // real packet starts.
            return;
        }

        resp = handle_request(req);

        std::uint8_t out[kPacketSize];
        encode(resp, out);
        std::size_t sent = 0;
        while (sent < kPacketSize) {
            ssize_t written = ::send(client_fd, out + sent, kPacketSize - sent, 0);
            if (written <= 0) {
                return;
            }
            sent += static_cast<std::size_t>(written);
        }
    }
}

packet control_server::handle_request(const packet &req) {
    packet resp;
    resp.query_id = req.query_id;

    switch (static_cast<protocol_command>(req.command)) {
        case protocol_command::ping: {
            resp.code = static_cast<std::uint16_t>(protocol_code::ack);
            return resp;
        }

        case protocol_command::set_freq: {
            double hz = bits_to_double(req.arg);
            if (!m_forge.set_dds_frequency_hz(hz)) {
                resp.code = static_cast<std::uint16_t>(protocol_code::nack);
                return resp;
            }
            auto actual = m_forge.get_dds_frequency_hz();
            resp.code = static_cast<std::uint16_t>(protocol_code::ack);
            resp.arg = double_to_bits(actual ? *actual : hz);
            return resp;
        }

        case protocol_command::get_freq: {
            auto hz = m_forge.get_dds_frequency_hz();
            if (!hz) {
                resp.code = static_cast<std::uint16_t>(protocol_code::nack);
                return resp;
            }
            resp.code = static_cast<std::uint16_t>(protocol_code::ack);
            resp.arg = double_to_bits(*hz);
            return resp;
        }

        case protocol_command::enable:
        case protocol_command::disable: {
            bool on = static_cast<protocol_command>(req.command) == protocol_command::enable;
            if (!m_forge.set_dds_enabled(on)) {
                resp.code = static_cast<std::uint16_t>(protocol_code::nack);
                return resp;
            }
            resp.code = static_cast<std::uint16_t>(protocol_code::ack);
            return resp;
        }

        case protocol_command::get_enabled: {
            auto en = m_forge.dds_enabled();
            if (!en) {
                resp.code = static_cast<std::uint16_t>(protocol_code::nack);
                return resp;
            }
            resp.code = static_cast<std::uint16_t>(protocol_code::ack);
            resp.arg = *en ? 1 : 0;
            return resp;
        }
    }

    resp.code = static_cast<std::uint16_t>(protocol_code::nack_unknown_command);
    return resp;
}

} // namespace net
