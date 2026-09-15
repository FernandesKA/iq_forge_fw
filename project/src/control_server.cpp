#include "control_server.h"

#include "iq_forge.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace net {

namespace {

std::string trim(const std::string &s) {
    std::size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return "";
    }
    std::size_t end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

// %.6f rather than iostream's default (which switches to scientific
// notation past 6 significant digits -- DDS frequencies routinely have 7-8).
std::string format_hz(double hz) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6f", hz);
    return buf;
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
    std::string buf;
    char chunk[256];
    while (m_running.load()) {
        ssize_t n = ::recv(client_fd, chunk, sizeof(chunk), 0);
        if (n <= 0) {
            return; // client disconnected or socket error
        }
        buf.append(chunk, static_cast<std::size_t>(n));

        std::size_t pos;
        while ((pos = buf.find('\n')) != std::string::npos) {
            std::string line = buf.substr(0, pos);
            buf.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                continue;
            }

            std::string response = handle_line(line);
            response += '\n';
            std::size_t sent = 0;
            while (sent < response.size()) {
                ssize_t written = ::send(client_fd, response.data() + sent, response.size() - sent, 0);
                if (written <= 0) {
                    return;
                }
                sent += static_cast<std::size_t>(written);
            }
        }
    }
}

std::string control_server::handle_line(const std::string &line) {
    std::string cmd = line;
    std::string arg;
    std::size_t sp = line.find(' ');
    if (sp != std::string::npos) {
        cmd = line.substr(0, sp);
        arg = trim(line.substr(sp + 1));
    }

    if (cmd == "PING") {
        return "OK";
    }

    if (cmd == "SET_FREQ") {
        if (arg.empty()) {
            return "ERR missing frequency";
        }
        char *end = nullptr;
        double hz = std::strtod(arg.c_str(), &end);
        if (end == arg.c_str()) {
            return "ERR invalid frequency";
        }
        if (!m_forge.set_dds_frequency_hz(hz)) {
            return "ERR " + m_forge.dds_ftw_gpio_error();
        }
        auto actual = m_forge.get_dds_frequency_hz();
        return "OK " + format_hz(actual ? *actual : hz);
    }

    if (cmd == "GET_FREQ") {
        auto hz = m_forge.get_dds_frequency_hz();
        if (!hz) {
            return "ERR " + m_forge.dds_ftw_gpio_error();
        }
        return "OK " + format_hz(*hz);
    }

    if (cmd == "ENABLE" || cmd == "DISABLE") {
        bool on = (cmd == "ENABLE");
        if (!m_forge.set_dds_enabled(on)) {
            return "ERR " + m_forge.dds_ctrl_gpio_error();
        }
        return "OK";
    }

    if (cmd == "GET_ENABLED") {
        auto en = m_forge.dds_enabled();
        if (!en) {
            return "ERR " + m_forge.dds_ctrl_gpio_error();
        }
        return std::string("OK ") + (*en ? "1" : "0");
    }

    return "ERR unknown command";
}

} // namespace net
