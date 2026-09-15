/**
 * @file control_server.h
 * @brief Binary TCP control server for iq_forge_app. Lets a host tool
 *        (iq_forge_gui's "iq_forge" device backend) drive the on-board DDS
 *        remotely over Ethernet -- frequency + enable/disable, for now
 *        (sine-only: the DDS itself runs entirely inside the FPGA, nothing
 *        is streamed over the network).
 *
 *        Wire protocol: fixed 20-byte packets in both directions, all
 *        multi-byte integers in network (big-endian) byte order. The same
 *        layout is used for requests and responses -- see
 *        iq_forge_gui/common/devices/inc/iq_forge_device.h for the
 *        client-side mirror of this format; keep the two in sync by hand
 *        (separate repos, no shared header).
 *
 *          offset  size  field
 *          0       4     magic     (kProtocolMagic, checked on every packet)
 *          4       2     command   (protocol_command; request only, 0 in responses)
 *          6       2     code      (0 in requests; protocol_code in responses)
 *          8       4     query_id  (client-assigned; echoed back verbatim)
 *          12      8     arg       (command-specific, see protocol_command)
 *
 *        Every request gets exactly one response with the same query_id,
 *        so a client can pipeline requests and match responses by id (this
 *        server itself still handles one connection, and within it one
 *        request, at a time -- query_id is for the client's benefit, not
 *        needed to disambiguate on this end).
 *
 *        Known limitation: commands are applied to the same project::iq_forge
 *        instance the interactive console menu (main.cpp's run_menu) also
 *        drives, with no locking between the two -- fine for the expected
 *        usage (either debug over the console XOR control from the GUI, not
 *        both at once), but a real data race if both happen concurrently.
 */

#pragma once

#include <atomic>
#include <cstddef>
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

constexpr std::uint32_t kProtocolMagic = 0x49514631; // "IQF1"
constexpr std::size_t kPacketSize = 20;

enum class protocol_command : std::uint16_t {
    ping = 0,
    set_freq = 1,
    get_freq = 2,
    enable = 3,
    disable = 4,
    get_enabled = 5,
};

enum class protocol_code : std::uint16_t {
    ack = 0,
    nack = 1,                   // generic failure (hardware read/write error)
    nack_unknown_command = 2,
    nack_bad_arg = 3,
};

// One 20-byte packet, decoded. `arg`'s meaning depends on `command` (on a
// request) and is documented per-command in the header comment above.
struct packet {
    std::uint32_t magic = kProtocolMagic;
    std::uint16_t command = 0;
    std::uint16_t code = 0;
    std::uint32_t query_id = 0;
    std::uint64_t arg = 0;
};

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
        // Applies one already-magic/decoded request packet, returning the
        // response packet to send back (same query_id, code + arg filled in).
        packet handle_request(const packet &req);

        project::iq_forge &m_forge;
        std::uint16_t m_port;
        int m_listen_fd = -1;
        std::thread m_thread;
        std::atomic<bool> m_running{false};
};

} // namespace net
