#include "WebServer.h"  // Include main header instead of individual header
#include <libwebsockets.h>
#include <queue>
#include <map>
#include <mutex>
#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>

// WebSocket session tracking
struct WebSocketSession {
    lws *wsi;
    std::queue<String> outgoing_messages;
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

        // Serve static files from src/www directory
        String file_path = "/common/active/sblo/Dev/VfsBoot/src/www";
        if (strcmp(requested_uri, "/") == 0) {
            file_path.Cat("/index.html");
        } else {
            file_path.Cat(requested_uri);
        }

        // Check if file exists
        std::ifstream file(String(file_path).ToStd());
        if (!file.good()) {
            // 404 for non-existent files
            lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, nullptr);
            return -1;
        }

        // Read file content
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        
        String content = buffer.str().c_str();
        std::cout << "[WebServer] Serving file: " << String(file_path).ToStd() << " (" << content.GetCount() << " bytes)" << std::endl;

        // Determine content type based on file extension
        String content_type = "text/plain";
        if (file_path.Right(5) == ".html") {
            content_type = "text/html";
        } else if (file_path.Right(4) == ".css") {
            content_type = "text/css";
        } else if (file_path.Right(3) == ".js") {
            content_type = "application/javascript";
        }

        // Prepare HTTP headers
        unsigned char http_headers[2048];
        unsigned char *p = &http_headers[0];
        unsigned char *end = &http_headers[sizeof(http_headers) - 1];

        // Write HTTP status and headers
        if (lws_add_http_header_status(wsi, HTTP_STATUS_OK, &p, end))
            return 1;
        
        // Add content type header
        if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE, 
                                        (unsigned char*)content_type.ToStd().c_str(), 
                                        content_type.GetCount(), &p, end))
            return 1;
            
        // Add content length header
        String content_length = IntStr(content.GetCount());
        if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_LENGTH,
                                        (unsigned char*)content_length.ToStd().c_str(),
                                        content_length.GetCount(), &p, end))
            return 1;

        // Finalize headers
        if (lws_finalize_http_header(wsi, &p, end))
            return 1;

        // Write headers
        if (lws_write(wsi, http_headers, p - http_headers, LWS_WRITE_HTTP_HEADERS) != (int)(p - http_headers))
            return 1;

        // Write content
        if (lws_write(wsi, (unsigned char*)content.ToStd().c_str(), content.GetCount(), LWS_WRITE_HTTP_FINAL) != (int)content.GetCount())
            return 1;

        // Complete HTTP transaction
        if (lws_http_transaction_completed(wsi))
            return -1;
            
        return 0;
    }

    case LWS_CALLBACK_HTTP_WRITEABLE: {
        // Serve static files from src/www directory
        char *requested_uri = (char *)in;
        
        // Construct file path
        String file_path = "/common/active/sblo/Dev/VfsBoot/src/www";
        if (strcmp(requested_uri, "/") == 0) {
            file_path.Cat("/index.html");
        } else {
            // Prevent directory traversal attacks by sanitizing the URI
            String uri_str = requested_uri;
            if (uri_str.Find("..") >= 0) {
                lws_return_http_status(wsi, HTTP_STATUS_FORBIDDEN, nullptr);
                return -1;
            }
            // Ensure we don't add double slashes
            if (uri_str.GetCount() > 0 && uri_str[0] != '/') {
                file_path.Cat("/");
            }
            file_path.Cat(requested_uri);
        }
        
        std::cout << "[WebServer] Serving file: " << String(file_path).ToStd() << std::endl;
        
        // Check if file exists
        std::ifstream file(String(file_path).ToStd());
        if (!file.good()) {
            std::cout << "[WebServer] File not found: " << String(file_path).ToStd() << std::endl;
            // 404 for non-existent files
            lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, nullptr);
            return -1;
        }
        
        // Read file content
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        
        String content = buffer.str().c_str();
        std::cout << "[WebServer] File content length: " << content.GetCount() << std::endl;

        // Determine content type based on file extension
        String content_type = "text/plain";
        if (file_path.Right(5) == ".html") {
            content_type = "text/html";
        } else if (file_path.Right(4) == ".css") {
            content_type = "text/css";
        } else if (file_path.Right(3) == ".js") {
            content_type = "application/javascript";
        }

        // Prepare HTTP headers
        unsigned char http_headers[2048];
        unsigned char *p = &http_headers[0];
        unsigned char *end = &http_headers[sizeof(http_headers) - 1];

        // Write HTTP status and headers
        if (lws_add_http_header_status(wsi, HTTP_STATUS_OK, &p, end))
            return 1;
        
        // Add content type header
        if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE, 
                                        (unsigned char*)content_type.ToStd().c_str(), 
                                        content_type.GetCount(), &p, end))
            return 1;
            
        // Add content length header
        String content_length = IntStr(content.GetCount());
        if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_LENGTH,
                                        (unsigned char*)content_length.ToStd().c_str(),
                                        content_length.GetCount(), &p, end))
            return 1;

        // Finalize headers
        if (lws_finalize_http_header(wsi, &p, end))
            return 1;

        // Write headers
        if (lws_write(wsi, http_headers, p - http_headers, LWS_WRITE_HTTP_HEADERS) != (int)(p - http_headers))
            return 1;

        // Write content
        if (lws_write(wsi, (unsigned char*)content.ToStd().c_str(), content.GetCount(), LWS_WRITE_HTTP_FINAL) != (int)content.GetCount())
            return 1;

        // Complete HTTP transaction
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
        String command = String().Cat() << std::string((char*)in, len);

        // Strip trailing newline if present
        while (command.GetCount() > 0 && (command[command.GetCount()-1] == '\n' || command[command.GetCount()-1] == '\r')) {
            command.Trim(command.GetCount()-1);
        }

        std::cout << "[WebServer] Received command: " << command.ToStd() << std::endl;

        // Execute command through callback
        if (g_command_callback) {
            try {
                auto [success, output] = g_command_callback(command);

                // Convert \n to \r\n for proper terminal display
                String terminal_output;
                terminal_output = output;
                // For U++ String, we need to do character replacement differently
                for (int i = 0; i < terminal_output.GetCount(); i++) {
                    if (terminal_output[i] == '\n' && (i == 0 || terminal_output[i-1] != '\r')) {
                        terminal_output.Insert(i, "\r");
                        i++; // Skip the inserted character
                    }
                }

                // Send output back to client
                // Split large messages into chunks to avoid buffer overflow and UTF-8 truncation
                {
                    std::lock_guard<std::mutex> msg_lock(session->mutex);
                    if (terminal_output.GetCount() > 0) {
                        const int CHUNK_SIZE = 4000; // Leave room for safety
                        int offset = 0;
                        while (offset < terminal_output.GetCount()) {
                            int chunk_len = std::min(CHUNK_SIZE, terminal_output.GetCount() - offset);

                            // Make sure we don't split a UTF-8 character
                            if (offset + chunk_len < terminal_output.GetCount()) {
                                unsigned char byte_at_boundary = terminal_output[offset + chunk_len];

                                // If we're about to split in the middle of a continuation byte, back up
                                while (chunk_len > 0 && (byte_at_boundary & 0xC0) == 0x80) {
                                    chunk_len--;
                                    if (offset + chunk_len < terminal_output.GetCount()) {
                                        byte_at_boundary = terminal_output[offset + chunk_len];
                                    } else {
                                        break;
                                    }
                                }

                                // Now check if the last byte in our chunk starts a multibyte sequence
                                // that would be incomplete
                                if (chunk_len > 0 && offset + chunk_len - 1 < terminal_output.GetCount()) {
                                    unsigned char last_byte = terminal_output[offset + chunk_len - 1];
                                    int bytes_needed = 0;

                                    if ((last_byte & 0xF8) == 0xF0) bytes_needed = 4; // 4-byte UTF-8
                                    else if ((last_byte & 0xF0) == 0xE0) bytes_needed = 3; // 3-byte UTF-8
                                    else if ((last_byte & 0xE0) == 0xC0) bytes_needed = 2; // 2-byte UTF-8

                                    // Check if we have all the bytes we need
                                    if (bytes_needed > 0) {
                                        int bytes_available = terminal_output.GetCount() - (offset + chunk_len - 1);
                                        if (bytes_available < bytes_needed) {
                                            // Incomplete sequence - back up to before it starts
                                            chunk_len--;
                                        }
                                    }
                                }
                            }

                            if (chunk_len > 0) {
                                String substr = terminal_output.Mid(offset, chunk_len);
                                session->outgoing_messages.push(substr);
                                offset += chunk_len;
                            } else {
                                // Safeguard: if we can't find a valid boundary, just send one character
                                String substr = terminal_output.Mid(offset, 1);
                                session->outgoing_messages.push(substr);
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

        String msg = session->outgoing_messages.front();
        session->outgoing_messages.pop();

        unsigned char buffer[LWS_PRE + 4096];
        int msg_len = msg.GetCount();

        // Ensure we don't exceed buffer size and preserve UTF-8 boundaries
        if (msg_len > 4095) {
            msg_len = 4095;
            // Back up to nearest UTF-8 character boundary
            while (msg_len > 0 && (msg[msg_len] & 0xC0) == 0x80) {
                msg_len--;
            }
        }

        memcpy(buffer + LWS_PRE, msg.ToStd().c_str(), msg_len);

        // Debug: Check for invalid UTF-8 sequences
        bool valid_utf8 = true;
        for (int i = 0; i < msg_len; i++) {
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
            for (int i = 0; i < std::min(msg_len, int(100)); i++) {
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

void send_output(const String& output) {
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
