/**
 * @file control_server.h
 * @brief Minimal line-based TCP control server for iq_forge_app. Lets a host
 *        tool (iq_forge_gui's "iq_forge" device backend) drive the on-board
 *        DDS remotely -- frequency + enable/disable, for now (sine-only:
 *        the DDS itself runs entirely inside the FPGA, nothing is streamed
 *        over the network).
 *
 *        Protocol: newline-terminated ASCII commands, one per line, a
 *        single active connection served at a time.
 *
 *          PING                -> OK
 *          SET_FREQ <hz>       -> OK <actual_hz>   | ERR <message>
 *          GET_FREQ            -> OK <hz>          | ERR <message>
 *          ENABLE              -> OK               | ERR <message>
 *          DISABLE             -> OK               | ERR <message>
 *          GET_ENABLED         -> OK 1|0           | ERR <message>
 *          (anything else)     -> ERR unknown command
 *
 *        Known limitation: commands are applied to the same project::iq_forge
 *        instance the interactive console menu (main.cpp's run_menu) also
 *        drives, with no locking between the two -- fine for the expected
 *        usage (either debug over the console XOR control from the GUI, not
 *        both at once), but a real data race if both happen concurrently.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace project {
class iq_forge;
}

namespace net {

// Default TCP port the control server listens on; iq_forge_gui's IqForgeDevice
// connects to this same port by default (see its uri parsing: "host[:port]").
constexpr std::uint16_t kDefaultControlPort = 7373;

class control_server {
    public:
        control_server(project::iq_forge &forge, std::uint16_t port = kDefaultControlPort);
        ~control_server();

        control_server(const control_server &) = delete;
        control_server &operator=(const control_server &) = delete;

        // Creates the listening socket and starts the accept loop on a
        // background thread. Returns false (with error_out set) on failure;
        // the caller can carry on without network control in that case.
        bool start(std::string &error_out);

        // Signals the accept loop to stop and joins its thread. Safe to call
        // even if start() was never called or failed. Also called from the
        // destructor.
        void stop();

        std::uint16_t port() const { return m_port; }

    private:
        void accept_loop();
        void handle_client(int client_fd);
        std::string handle_line(const std::string &line);

        project::iq_forge &m_forge;
        std::uint16_t m_port;
        int m_listen_fd = -1;
        std::thread m_thread;
        std::atomic<bool> m_running{false};
};

} // namespace net
