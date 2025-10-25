// Terminal functionality for VfsBoot web interface

class VfsBootTerminal {
    constructor() {
        this.term = null;
        this.ws = null;
        this.inputBuffer = '';
        this.isConnected = false;
        this.initializeTerminal();
        this.initializeWebSocket();
        this.setupEventListeners();
    }
    
    initializeTerminal() {
        // Initialize xterm.js terminal
        this.term = new Terminal({
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
        this.term.loadAddon(fitAddon);
        this.term.open(document.getElementById('terminal'));
        fitAddon.fit();

        // Handle window resize
        window.addEventListener('resize', () => {
            fitAddon.fit();
        });
    }
    
    initializeWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        this.ws = new WebSocket(`${protocol}//${window.location.host}/ws`, 'ws-terminal');
        
        this.ws.onopen = () => {
            this.isConnected = true;
            this.updateStatus(true);
            this.term.writeln('\x1b[32m╔═══════════════════════════════════════════════════════════╗\x1b[0m');
            this.term.writeln('\x1b[32m║\x1b[0m  \x1b[1;36mWelcome to VfsBoot Web Interface\x1b[0m                    \x1b[32m║\x1b[0m');
            this.term.writeln('\x1b[32m║\x1b[0m  Type \x1b[33mhelp\x1b[0m for available commands                     \x1b[32m║\x1b[0m');
            this.term.writeln('\x1b[32m╚═══════════════════════════════════════════════════════════╝\x1b[0m');
            this.term.write('\r\n\x1b[36mcodex> \x1b[0m ');
        };

        this.ws.onclose = () => {
            this.isConnected = false;
            this.updateStatus(false);
            this.term.writeln('\r\n\x1b[31m[Connection closed]\x1b[0m');
        };

        this.ws.onerror = (error) => {
            this.isConnected = false;
            this.updateStatus(false);
            this.term.writeln('\r\n\x1b[31m[WebSocket error]\x1b[0m');
            console.error('WebSocket error:', error);
        };

        this.ws.onmessage = (event) => {
            this.term.write(event.data);
        };
    }
    
    updateStatus(connected) {
        const statusIndicator = document.getElementById('status-indicator');
        const statusText = document.getElementById('status-text');
        
        if (connected) {
            statusIndicator.classList.add('connected');
            statusText.textContent = 'Connected';
        } else {
            statusIndicator.classList.remove('connected');
            statusText.textContent = 'Disconnected';
        }
    }
    
    sendCommand(command) {
        if (this.isConnected && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(command + '\n');
            return true;
        }
        return false;
    }
    
    setupEventListeners() {
        // Terminal input handling
        this.term.onData(data => {
            // Handle special keys
            if (data === '\r') { // Enter key
                this.sendCommand(this.inputBuffer);
                this.term.write('\r\n');
                this.inputBuffer = '';
            } else if (data === '\x7f') { // Backspace
                if (this.inputBuffer.length > 0) {
                    this.inputBuffer = this.inputBuffer.slice(0, -1);
                    this.term.write('\b \b');
                }
            } else if (data === '\x03') { // Ctrl+C
                this.ws.send('\x03');
                this.inputBuffer = '';
                this.term.write('^C\r\n\x1b[36mcodex> \x1b[0m ');
            } else if (data.charCodeAt(0) < 32) { // Ignore other control chars for now
                // TODO: Handle Ctrl+U, Ctrl+K, arrow keys, etc.
            } else {
                this.inputBuffer += data;
                this.term.write(data);
            }
        });
        
        // Command input handling
        const commandInput = document.getElementById('command-input');
        const sendBtn = document.getElementById('send-btn');
        const clearBtn = document.getElementById('clear-btn');
        const saveBtn = document.getElementById('save-btn');
        
        sendBtn.addEventListener('click', () => {
            if (commandInput.value.trim()) {
                this.sendCommand(commandInput.value);
                commandInput.value = '';
            }
        });
        
        commandInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                sendBtn.click();
            }
        });
        
        clearBtn.addEventListener('click', () => {
            this.term.clear();
        });
        
        saveBtn.addEventListener('click', () => {
            this.term.writeln('\r\n\x1b[33mSaving session...\x1b[0m');
            // TODO: Implement session saving
        });
        
        // Sidebar navigation
        document.querySelectorAll('.sidebar-item').forEach(item => {
            item.addEventListener('click', () => {
                // Remove active class from all items
                document.querySelectorAll('.sidebar-item').forEach(i => {
                    i.classList.remove('active');
                });
                
                // Add active class to clicked item
                item.classList.add('active');
                
                // Change view based on data-view attribute
                const view = item.getAttribute('data-view');
                this.term.writeln(`\r\n\x1b[33mSwitching to ${view} view...\x1b[0m`);
                // TODO: Implement view switching
            });
        });
    }
}

// Initialize terminal when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    new VfsBootTerminal();
});