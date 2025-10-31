# Remote VFS Mounting

VfsBoot supports mounting remote VFS filesystems over TCP, enabling transparent access to VFS content on remote vfsh instances.

## Architecture

The remote VFS system consists of:

- **RemoteNode**: Client-side VFS node that communicates with remote server
- **Daemon Mode**: Server mode that accepts incoming remote mount connections
- **EXEC Protocol**: Simple line-based protocol for executing commands remotely

## Protocol

Communication uses a simple request-response protocol:

### Request Format
```
EXEC <command>\n
```

### Response Format
```
OK <output>\n
```
or
```
ERR <error-message>\n
```

Commands are executed via shell (`popen()`) on the server, allowing access to both VFS commands and system commands.

## Usage

### Starting a Daemon Server

Run codex in daemon mode to accept remote connections:

```bash
./vfsh --daemon 9999
```

This starts a server listening on port 9999. The server will:
- Accept TCP connections from remote clients
- Execute commands sent via the EXEC protocol
- Return command output or errors
- Handle multiple concurrent connections (one thread per connection)

### Mounting Remote VFS from Client

From another codex instance (or same machine), mount the remote VFS:

```bash
# In codex shell
mount.remote localhost 9999 / /remote

# Now access remote VFS
ls /remote
cat /remote/src/file.txt
```

### Command Syntax

```
mount.remote <host> <port> <remote-vfs-path> <local-vfs-path>
```

- **host**: Server hostname or IP address
- **port**: Server daemon port number
- **remote-vfs-path**: VFS path on the remote server to mount (use `/` for root)
- **local-vfs-path**: Local VFS path where remote will appear

### Viewing Mounts

```bash
mount.list
```

Output shows mount type markers:
- `m` = filesystem mount
- `l` = library mount
- `r` = remote mount

### Unmounting

```bash
unmount /remote
```

## Use Cases

### 1. Accessing Remote VFS Content

```bash
# Server
./vfsh --daemon 9999

# Client
mount.remote server.example.com 9999 / /remote
tree /remote
```

### 2. Copying Between Real Filesystems on Different Hosts

This is the primary use case mentioned in TASKS.md:

```bash
# Host A (192.168.1.100)
./vfsh --daemon 9999
mount /path/to/source /vfs/source

# Host B
./vfsh
mount /path/to/destination /vfs/dest
mount.remote 192.168.1.100 9999 /vfs/source /remote/source

# Now copy files
cat /remote/source/file.txt > /vfs/dest/file.txt
```

This copies `/path/to/source/file.txt` on Host A to `/path/to/destination/file.txt` on Host B.

### 3. Remote Command Execution

Since the protocol executes shell commands, you can run any command on the remote server:

```bash
mount.remote localhost 9999 / /remote
# The remote server executes: ls /
cat /remote/  # Lists remote VFS root
```

## Implementation Details

### RemoteNode Class

Located in `VfsShell/codex.h` and `VfsShell/codex.cpp`:

- Inherits from `VfsNode`
- Maintains TCP socket connection to server
- Implements `read()`, `write()`, `isDir()`, `children()` via remote EXEC
- Thread-safe with mutex-protected socket operations
- Lazy connection establishment
- Result caching for directory listings

### Daemon Server

Located in `VfsShell/codex.cpp`:

- Function: `run_daemon_server(int port, ...)`
- Binds to `INADDR_ANY` (all interfaces)
- Spawns detached thread per client connection
- Executes commands via `popen()`
- Returns stdout/stderr to client

### Network Communication

- Uses standard BSD sockets (AF_INET, SOCK_STREAM)
- Hostname resolution via `gethostbyname()`
- Line-buffered protocol (reads until newline)
- No encryption (use SSH tunneling for secure connections)

## Limitations

1. **No Authentication**: Server accepts all connections (use firewall rules)
2. **No Encryption**: Traffic sent in plain text
3. **Command Injection**: Remote commands executed via shell (security risk)
4. **No Pipelining**: One request/response at a time per connection
5. **Caching**: Directory listings cached until invalidated

## Security Considerations

⚠️ **WARNING**: The daemon mode executes arbitrary commands sent by clients. Only use on trusted networks or behind proper firewall rules.

Recommendations:
- Bind to localhost only: modify code to use `INADDR_LOOPBACK`
- Use SSH tunneling: `ssh -L 9999:localhost:9999 remote-host`
- Add authentication layer
- Implement command whitelisting

## Future Enhancements

See TASKS.md for planned improvements:

- SSH/SFTP transport layer
- Password and key-based authentication
- Background thread for network I/O (currently synchronous)
- Proper VFS command dispatch instead of shell execution
- TLS/SSL encryption support

## Examples

See example scripts:
- `scripts/examples/remote-mount-demo.cx` - Basic remote mount demonstration
- `scripts/examples/remote-copy-demo.cx` - File copying between hosts

## Troubleshooting

### Connection Refused
- Ensure daemon is running: `./vfsh --daemon 9999`
- Check firewall allows port 9999
- Verify hostname resolution

### Command Timeouts
- Check network latency
- Ensure remote command doesn't hang

### Permission Denied
- Daemon runs with same permissions as user
- Remote filesystem access follows daemon user's permissions

### Cache Issues
- `unmount` and `mount.remote` again to refresh cache
- Directory caching invalidated on write operations
