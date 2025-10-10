// web_server.cpp - Lightweight HTTP/WebSocket server for VfsBoot
// Built with libwebsockets for browser-based terminal access

#include "codex.h"
#include <libwebsockets.h>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <thread>

// WebSocket session tracking
struct WebSocketSession {
    lws *wsi;
    std::queue<std::string> outgoing_messages;
    std::mutex mutex;
};

// Global state for web server
static std::map<lws*, WebSocketSession*> g_sessions;
static std::mutex g_sessions_mutex;
static lws_context* g_lws_context = nullptr;
static bool g_server_running = false;
static std::thread g_server_thread;
static WebServer::CommandCallback g_command_callback;

// Embedded HTML page with xterm.js terminal
static const char* INDEX_HTML = R"HTML(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>VfsBoot Terminal</title>
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/xterm@5.3.0/css/xterm.css" />
    <style>
        body {
            margin: 0;
            padding: 0;
            background: #1e1e1e;
            font-family: 'Consolas', 'Monaco', monospace;
            overflow: hidden;
        }
        #header {
            background: #2d2d30;
            color: #cccccc;
            padding: 10px 20px;
            border-bottom: 1px solid #3e3e42;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        #header h1 {
            margin: 0;
            font-size: 16px;
            font-weight: 600;
        }
        #status {
            display: flex;
            align-items: center;
            gap: 8px;
        }
        #status-indicator {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background: #f48771;
        }
        #status-indicator.connected {
            background: #89d185;
        }
        #terminal-container {
            position: absolute;
            top: 50px;
            left: 0;
            right: 0;
            bottom: 0;
            padding: 10px;
        }
        #terminal {
            height: 100%;
        }
    </style>
</head>
<body>
    <div id="header">
        <h1>🤖 VfsBoot Terminal</h1>
        <div id="status">
            <div id="status-indicator"></div>
            <span id="status-text">Connecting...</span>
        </div>
    </div>
    <div id="terminal-container">
        <div id="terminal"></div>
    </div>

    <script src="https://cdn.jsdelivr.net/npm/xterm@5.3.0/lib/xterm.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/xterm-addon-fit@0.8.0/lib/xterm-addon-fit.js"></script>
    <script>
        // Initialize xterm.js terminal
        const term = new Terminal({
            cursorBlink: true,
            fontSize: 14,
            fontFamily: '"Cascadia Code", Consolas, Monaco, monospace',
            theme: {
                background: '#1e1e1e',
                foreground: '#d4d4d4',
                cursor: '#d4d4d4',
                selection: '#264f78',
                black: '#000000',
                red: '#cd3131',
                green: '#0dbc79',
                yellow: '#e5e510',
                blue: '#2472c8',
                magenta: '#bc3fbc',
                cyan: '#11a8cd',
                white: '#e5e5e5',
                brightBlack: '#666666',
                brightRed: '#f14c4c',
                brightGreen: '#23d18b',
                brightYellow: '#f5f543',
                brightBlue: '#3b8eea',
                brightMagenta: '#d670d6',
                brightCyan: '#29b8db',
                brightWhite: '#ffffff'
            }
        });

        const fitAddon = new FitAddon.FitAddon();
        term.loadAddon(fitAddon);
        term.open(document.getElementById('terminal'));
        fitAddon.fit();

        // Status indicators
        const statusIndicator = document.getElementById('status-indicator');
        const statusText = document.getElementById('status-text');

        function setStatus(connected) {
            if (connected) {
                statusIndicator.classList.add('connected');
                statusText.textContent = 'Connected';
            } else {
                statusIndicator.classList.remove('connected');
                statusText.textContent = 'Disconnected';
            }
        }

        // WebSocket connection
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const ws = new WebSocket(`${protocol}//${window.location.host}/ws`, 'ws-terminal');
        let inputBuffer = '';

        ws.onopen = () => {
            setStatus(true);
            term.writeln('\x1b[32m╔═══════════════════════════════════════════════════════════╗\x1b[0m');
            term.writeln('\x1b[32m║\x1b[0m  \x1b[1;36mWelcome to VfsBoot Web Terminal\x1b[0m                      \x1b[32m║\x1b[0m');
            term.writeln('\x1b[32m║\x1b[0m  Type \x1b[33mhelp\x1b[0m for available commands                     \x1b[32m║\x1b[0m');
            term.writeln('\x1b[32m╚═══════════════════════════════════════════════════════════╝\x1b[0m');
            term.write('\r\n\x1b[36mcodex>\x1b[0m ');
        };

        ws.onclose = () => {
            setStatus(false);
            term.writeln('\r\n\x1b[31m[Connection closed]\x1b[0m');
        };

        ws.onerror = (error) => {
            setStatus(false);
            term.writeln('\r\n\x1b[31m[WebSocket error]\x1b[0m');
            console.error('WebSocket error:', error);
        };

        // Add debug logging
        console.log('Attempting WebSocket connection to:', `${protocol}//${window.location.host}/ws`);

        ws.onmessage = (event) => {
            term.write(event.data);
        };

        // Terminal input handling
        term.onData(data => {
            // Handle special keys
            if (data === '\r') { // Enter key
                ws.send(inputBuffer + '\n');
                term.write('\r\n');
                inputBuffer = '';
            } else if (data === '\x7f') { // Backspace
                if (inputBuffer.length > 0) {
                    inputBuffer = inputBuffer.slice(0, -1);
                    term.write('\b \b');
                }
            } else if (data === '\x03') { // Ctrl+C
                ws.send('\x03');
                inputBuffer = '';
                term.write('^C\r\n\x1b[36mcodex>\x1b[0m ');
            } else if (data.charCodeAt(0) < 32) { // Ignore other control chars for now
                // TODO: Handle Ctrl+U, Ctrl+K, arrow keys, etc.
            } else {
                inputBuffer += data;
                term.write(data);
            }
        });

        // Handle window resize
        window.addEventListener('resize', () => {
            fitAddon.fit();
            // TODO: Send terminal size to backend
        });

        // Initial fit
        setTimeout(() => fitAddon.fit(), 100);
    </script>
</body>
</html>
)HTML";

// HTTP callback for serving static content
static int callback_http(lws *wsi, enum lws_callback_reasons reason,
                         void *user, void *in, size_t len) {
    switch (reason) {
    case LWS_CALLBACK_HTTP: {
        char *requested_uri = (char *)in;
        std::cout << "[WebServer] HTTP request for: " << requested_uri << std::endl;

        // WebSocket upgrade for /ws - just return 0 to let libwebsockets handle it
        if (strcmp(requested_uri, "/ws") == 0) {
            std::cout << "[WebServer] WebSocket upgrade requested for /ws" << std::endl;
            return 0;
        }

        // Serve index.html for root path
        if (strcmp(requested_uri, "/") == 0 || strcmp(requested_uri, "/index.html") == 0) {
            unsigned char buffer[LWS_PRE + 4096];
            unsigned char *p = &buffer[LWS_PRE];
            unsigned char *end = &buffer[sizeof(buffer) - 1];

            // Write HTTP headers
            if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK, "text/html",
                                           LWS_ILLEGAL_HTTP_CONTENT_LEN,
                                           &p, end))
                return 1;
            if (lws_finalize_write_http_header(wsi, buffer + LWS_PRE, &p, end))
                return 1;

            // Write body in chunks
            lws_callback_on_writable(wsi);
            return 0;
        }

        // 404 for other paths
        lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, nullptr);
        return -1;
    }

    case LWS_CALLBACK_HTTP_WRITEABLE: {
        // Send HTML content
        unsigned char buffer[LWS_PRE + 8192];
        int n = snprintf((char*)buffer + LWS_PRE, 8192, "%s", INDEX_HTML);

        if (lws_write(wsi, buffer + LWS_PRE, n, LWS_WRITE_HTTP_FINAL) != n)
            return 1;

        if (lws_http_transaction_completed(wsi))
            return -1;
        return 0;
    }

    default:
        break;
    }

    return 0;
}

// WebSocket callback for terminal communication
static int callback_websocket(lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len) {
    WebSocketSession *session = nullptr;

    std::cout << "[WebServer] WebSocket callback: reason=" << reason << std::endl;

    switch (reason) {
    case LWS_CALLBACK_ESTABLISHED: {
        // New WebSocket connection
        session = new WebSocketSession{wsi, {}, {}};

        std::lock_guard<std::mutex> lock(g_sessions_mutex);
        g_sessions[wsi] = session;

        std::cout << "[WebServer] New terminal session established" << std::endl;
        break;
    }

    case LWS_CALLBACK_CLOSED: {
        // Connection closed
        std::lock_guard<std::mutex> lock(g_sessions_mutex);
        auto it = g_sessions.find(wsi);
        if (it != g_sessions.end()) {
            delete it->second;
            g_sessions.erase(it);
        }

        std::cout << "[WebServer] Terminal session closed" << std::endl;
        break;
    }

    case LWS_CALLBACK_RECEIVE: {
        // Received data from client
        std::lock_guard<std::mutex> lock(g_sessions_mutex);
        auto it = g_sessions.find(wsi);
        if (it == g_sessions.end())
            break;

        session = it->second;
        std::string command((char*)in, len);

        // Strip trailing newline if present
        while (!command.empty() && (command.back() == '\n' || command.back() == '\r')) {
            command.pop_back();
        }

        std::cout << "[WebServer] Received command: " << command << std::endl;

        // Execute command through callback
        if (g_command_callback) {
            try {
                auto [success, output] = g_command_callback(command);

                // Send output back to client
                // Split large messages into chunks to avoid buffer overflow and UTF-8 truncation
                {
                    std::lock_guard<std::mutex> msg_lock(session->mutex);
                    if (!output.empty()) {
                        const size_t CHUNK_SIZE = 4000; // Leave room for safety
                        size_t offset = 0;
                        while (offset < output.length()) {
                            size_t chunk_len = std::min(CHUNK_SIZE, output.length() - offset);

                            // Make sure we don't split a UTF-8 character
                            if (offset + chunk_len < output.length()) {
                                unsigned char byte_at_boundary = output[offset + chunk_len];

                                // If we're about to split in the middle of a continuation byte, back up
                                while (chunk_len > 0 && (byte_at_boundary & 0xC0) == 0x80) {
                                    chunk_len--;
                                    if (offset + chunk_len < output.length()) {
                                        byte_at_boundary = output[offset + chunk_len];
                                    } else {
                                        break;
                                    }
                                }

                                // Now check if the last byte in our chunk starts a multibyte sequence
                                // that would be incomplete
                                if (chunk_len > 0 && offset + chunk_len - 1 < output.length()) {
                                    unsigned char last_byte = output[offset + chunk_len - 1];
                                    size_t bytes_needed = 0;

                                    if ((last_byte & 0xF8) == 0xF0) bytes_needed = 4; // 4-byte UTF-8
                                    else if ((last_byte & 0xF0) == 0xE0) bytes_needed = 3; // 3-byte UTF-8
                                    else if ((last_byte & 0xE0) == 0xC0) bytes_needed = 2; // 2-byte UTF-8

                                    // Check if we have all the bytes we need
                                    if (bytes_needed > 0) {
                                        size_t bytes_available = output.length() - (offset + chunk_len - 1);
                                        if (bytes_available < bytes_needed) {
                                            // Incomplete sequence - back up to before it starts
                                            chunk_len--;
                                        }
                                    }
                                }
                            }

                            if (chunk_len > 0) {
                                session->outgoing_messages.push(output.substr(offset, chunk_len));
                                offset += chunk_len;
                            } else {
                                // Safeguard: if we can't find a valid boundary, just send one byte
                                session->outgoing_messages.push(output.substr(offset, 1));
                                offset++;
                            }
                        }
                    }
                    // Send prompt for next command
                    session->outgoing_messages.push("\x1b[36mcodex>\x1b[0m ");
                }

                lws_callback_on_writable(wsi);
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> msg_lock(session->mutex);
                session->outgoing_messages.push("\x1b[31merror: ");
                session->outgoing_messages.push(e.what());
                session->outgoing_messages.push("\x1b[0m\r\n\x1b[36mcodex>\x1b[0m ");
                lws_callback_on_writable(wsi);
            }
        } else {
            // No callback registered - echo for testing
            std::lock_guard<std::mutex> msg_lock(session->mutex);
            session->outgoing_messages.push("No command handler registered\r\n");
            session->outgoing_messages.push("\x1b[36mcodex>\x1b[0m ");
            lws_callback_on_writable(wsi);
        }

        break;
    }

    case LWS_CALLBACK_SERVER_WRITEABLE: {
        // Ready to send data to client
        std::lock_guard<std::mutex> lock(g_sessions_mutex);
        auto it = g_sessions.find(wsi);
        if (it == g_sessions.end())
            break;

        session = it->second;

        std::lock_guard<std::mutex> msg_lock(session->mutex);
        if (session->outgoing_messages.empty())
            break;

        std::string msg = session->outgoing_messages.front();
        session->outgoing_messages.pop();

        unsigned char buffer[LWS_PRE + 4096];
        size_t msg_len = msg.length();

        // Ensure we don't exceed buffer size and preserve UTF-8 boundaries
        if (msg_len > 4095) {
            msg_len = 4095;
            // Back up to nearest UTF-8 character boundary
            while (msg_len > 0 && (msg[msg_len] & 0xC0) == 0x80) {
                msg_len--;
            }
        }

        memcpy(buffer + LWS_PRE, msg.c_str(), msg_len);

        // Debug: Check for invalid UTF-8 sequences
        bool valid_utf8 = true;
        for (size_t i = 0; i < msg_len; i++) {
            unsigned char c = buffer[LWS_PRE + i];
            if ((c & 0x80) == 0) continue; // ASCII
            if ((c & 0xE0) == 0xC0) { // 2-byte sequence
                if (i + 1 >= msg_len || (buffer[LWS_PRE + i + 1] & 0xC0) != 0x80) {
                    valid_utf8 = false;
                    std::cerr << "[WebSocket] Invalid UTF-8 at byte " << i << ": 0x"
                              << std::hex << (int)c << std::dec << std::endl;
                    break;
                }
                i += 1;
            } else if ((c & 0xF0) == 0xE0) { // 3-byte sequence
                if (i + 2 >= msg_len || (buffer[LWS_PRE + i + 1] & 0xC0) != 0x80
                    || (buffer[LWS_PRE + i + 2] & 0xC0) != 0x80) {
                    valid_utf8 = false;
                    std::cerr << "[WebSocket] Invalid UTF-8 at byte " << i << ": 0x"
                              << std::hex << (int)c << std::dec << std::endl;
                    break;
                }
                i += 2;
            } else if ((c & 0xF8) == 0xF0) { // 4-byte sequence
                if (i + 3 >= msg_len || (buffer[LWS_PRE + i + 1] & 0xC0) != 0x80
                    || (buffer[LWS_PRE + i + 2] & 0xC0) != 0x80
                    || (buffer[LWS_PRE + i + 3] & 0xC0) != 0x80) {
                    valid_utf8 = false;
                    std::cerr << "[WebSocket] Invalid UTF-8 at byte " << i << ": 0x"
                              << std::hex << (int)c << std::dec << std::endl;
                    break;
                }
                i += 3;
            } else {
                valid_utf8 = false;
                std::cerr << "[WebSocket] Invalid UTF-8 byte at " << i << ": 0x"
                          << std::hex << (int)c << std::dec << std::endl;
                break;
            }
        }

        if (!valid_utf8) {
            std::cerr << "[WebSocket] Message contains invalid UTF-8, length=" << msg_len << std::endl;
            std::cerr << "[WebSocket] First 100 chars: ";
            for (size_t i = 0; i < std::min(msg_len, size_t(100)); i++) {
                unsigned char c = buffer[LWS_PRE + i];
                if (c >= 32 && c < 127) std::cerr << c;
                else std::cerr << "\\x" << std::hex << (int)c << std::dec;
            }
            std::cerr << std::endl;
        }

        lws_write(wsi, buffer + LWS_PRE, msg_len, LWS_WRITE_TEXT);

        // Schedule more writes if needed
        if (!session->outgoing_messages.empty())
            lws_callback_on_writable(wsi);
        break;
    }

    default:
        break;
    }

    return 0;
}

// Protocol definitions
static struct lws_protocols protocols[] = {
    {
        "",  // Default protocol (HTTP) - empty string means "handle everything not matched by other protocols"
        callback_http,
        0,
        0,
        0, nullptr, 0
    },
    {
        "ws-terminal",  // WebSocket protocol
        callback_websocket,
        0,
        4096,
        0, nullptr, 0
    },
    LWS_PROTOCOL_LIST_TERM
};

// Mount table for WebSocket endpoint
static struct lws_http_mount mount_ws = {
    nullptr,            // mount_next
    "/ws",              // mountpoint
    nullptr,            // origin
    nullptr,            // def
    "ws-terminal",      // protocol
    nullptr,            // cgienv
    nullptr,            // extra_mimetypes
    nullptr,            // interpret
    0,                  // cgi_timeout
    0,                  // cache_max_age
    0,                  // auth_mask
    0,                  // cache_reusable:1
    0,                  // cache_revalidate:1
    0,                  // cache_intermediaries:1
    0,                  // cache_no:1
    LWSMPRO_CALLBACK,   // origin_protocol
    3,                  // mountpoint_len ("/ws" = 3 chars)
    nullptr,            // basic_auth_login_file
    nullptr,            // cgi_chroot_path
    nullptr,            // cgi_wd
    nullptr,            // headers
    0                   // keepalive_timeout
};

// Server thread function
static void server_thread_func(int port) {
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.port = port;
    info.protocols = protocols;
    info.mounts = nullptr;      // Don't use mount for WebSocket - handle via protocol
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
                   LWS_SERVER_OPTION_VALIDATE_UTF8;

    g_lws_context = lws_create_context(&info);
    if (!g_lws_context) {
        std::cerr << "[WebServer] Failed to create libwebsockets context" << std::endl;
        return;
    }

    std::cout << "[WebServer] Listening on http://localhost:" << port << std::endl;
    std::cout << "[WebServer] Open browser to: http://localhost:" << port << "/" << std::endl;

    // Event loop
    while (g_server_running) {
        lws_service(g_lws_context, 50);
    }

    lws_context_destroy(g_lws_context);
    g_lws_context = nullptr;
}

// Public API
namespace WebServer {

bool start(int port) {
    if (g_server_running) {
        std::cerr << "[WebServer] Server already running" << std::endl;
        return false;
    }

    g_server_running = true;
    g_server_thread = std::thread(server_thread_func, port);

    return true;
}

void stop() {
    if (!g_server_running)
        return;

    g_server_running = false;

    if (g_server_thread.joinable())
        g_server_thread.join();

    // Clean up sessions
    std::lock_guard<std::mutex> lock(g_sessions_mutex);
    for (auto& pair : g_sessions) {
        delete pair.second;
    }
    g_sessions.clear();
}

bool is_running() {
    return g_server_running;
}

void send_output(const std::string& output) {
    // Broadcast output to all connected sessions
    std::lock_guard<std::mutex> lock(g_sessions_mutex);

    for (auto& pair : g_sessions) {
        WebSocketSession* session = pair.second;
        std::lock_guard<std::mutex> msg_lock(session->mutex);
        session->outgoing_messages.push(output);
        lws_callback_on_writable(pair.first);
    }
}

void set_command_callback(CommandCallback callback) {
    g_command_callback = callback;
}

} // namespace WebServer
