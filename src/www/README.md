# VfsBoot Web Interface

This directory contains the web interface files for VfsBoot's built-in web server.

## Structure

- `index.html` - Main entry point for the web interface
- `style.css` - Styling for the web interface
- `script.js` - JavaScript functionality for the web interface
- `assets/` - Directory for static assets (images, icons, etc.)

## Features

- Terminal interface with xterm.js
- File browser for navigating the VFS
- Settings panel for configuration
- WebSocket connection to VfsBoot backend
- Responsive design for different screen sizes

## Usage

To start the web interface, run:

```bash
./vfsh --web-server
```

Then open your browser to `http://localhost:8080/` (or the port specified).

## Development

The web interface connects to the VfsBoot backend via WebSocket. The backend serves the files from this directory and handles WebSocket communication for terminal commands.

Files are served statically from this directory, with `index.html` being the default page for the root path.