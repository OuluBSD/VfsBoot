# qwen Manager Mode Documentation

## Overview

The qwen Manager Mode is a sophisticated multi-repository AI project management system that enables hierarchical control of development workflows across multiple repositories and accounts. It provides an automated system where strategic, task, and repository-level management is handled by specialized AI sessions with appropriate models.

## Architecture

The manager mode implements a hierarchical architecture:

```
MANAGER (Management Repository)
├── PROJECT MANAGER (qwen-openai) - expensive, high quality
├── TASK MANAGER (qwen-auth) - cheaper, regular quality
│
├── ACCOUNT #1 (Computer A)
│   ├── REPO #1 (qwen-auth WORKER + qwen-openai MANAGER)
│   ├── REPO #2 (qwen-auth WORKER + qwen-openai MANAGER)
│   └── REPO #3 (qwen-auth WORKER + qwen-openai MANAGER)
│
└── ACCOUNT #2 (Computer B)
    ├── REPO #4 (qwen-auth WORKER + qwen-openai MANAGER)
    └── REPO #5 (qwen-auth WORKER + qwen-openai MANAGER)
```

### Session Types

1. **PROJECT MANAGER** (`qwen-openai`):
   - Handles strategic planning and architectural decisions
   - Uses expensive but high-quality AI model
   - Receives escalations from lower levels

2. **TASK MANAGER** (`qwen-auth`):
   - Coordinates implementation tasks
   - Uses regular quality AI model
   - Bridges strategic and implementation levels

3. **ACCOUNT**:
   - Represents remote computers connecting to the manager
   - Manages repository connections from that computer

4. **REPO MANAGER** (`qwen-openai`):
   - Handles complex repository tasks
   - Reviews work from REPO WORKER
   - Activated after WORKER failures

5. **REPO WORKER** (`qwen-auth`):
   - Handles routine implementation tasks
   - Uses regular quality AI model

## Configuration

The system is configured via `ACCOUNTS.json`:

```json
{
  "accounts": [
    {
      "id": "unique-account-identifier",
      "hostname": "computer-hostname-or-ip",
      "enabled": true,
      "max_concurrent_repos": 3,
      "repositories": [
        {
          "id": "unique-repo-identifier",
          "url": "git-repository-url",
          "local_path": "/local/path/to/clone",
          "enabled": true,
          "worker_model": "qwen-auth",
          "manager_model": "qwen-openai"
        }
      ]
    }
  ]
}
```

### Configuration Schema

- `accounts[].id`: Unique identifier for the account
- `accounts[].hostname`: Hostname or IP of the computer
- `accounts[].enabled`: Whether the account is active
- `accounts[].max_concurrent_repos`: Maximum number of repositories that can run simultaneously
- `accounts[].repositories[].id`: Unique identifier for the repository
- `accounts[].repositories[].url`: Git URL of the repository
- `accounts[].repositories[].local_path`: Local path where repository is cloned
- `accounts[].repositories[].enabled`: Whether the repository is active
- `accounts[].repositories[].worker_model`: AI model for worker sessions
- `accounts[].repositories[].manager_model`: AI model for manager sessions

## Usage

### Starting Manager Mode

```bash
# Start manager mode
qwen --manager
# or
qwen -m
```

### UI Navigation

The manager mode UI features:
- **Top section**: Session list (max 10 rows) showing all active sessions
- **Middle section**: Status separator bar
- **Bottom section**: Chat/command interface for the selected session

#### Keyboard Controls

- **TAB**: Switch focus between session list and input area
- **Arrow keys**: Navigate session list when focused
- **ENTER**: Select a session from the list
- **Page Up/Down**: Scroll through conversation history
- **Ctrl+U/D**: Scroll up/down through history
- **Ctrl+C**: Two-press exit pattern (first clears input, second exits)

### Session Commands

#### ACCOUNT Session Commands

When an ACCOUNT session is selected, the following technical commands are available without the `/` prefix:

- `list` - Show all repositories for the account
- `enable <repo_id>` - Enable a specific repository
- `disable <repo_id>` - Disable a specific repository
- `status <repo_id>` - Check repository status

#### REPO Session Controls

When a REPO session is selected:

- `/auto` - Return to automatic workflow mode (from manual override)

## Automation Workflows

### Failure Escalation

- REPO WORKER sessions track consecutive failures
- After 3 consecutive failures, the task is automatically escalated to the corresponding REPO MANAGER
- The MANAGER takes over until the task is completed successfully

### Test Interval Triggers

- REPO WORKER sessions count commits made during automatic operation
- After 3 commits, the REPO MANAGER is triggered to perform code review and comprehensive testing
- This ensures quality is maintained through regular oversight

### Manual Override Mode

- Users can take control of REPO sessions by typing directly in the input
- The `/auto` command returns the session to the automatic workflow
- This allows for human intervention when needed while maintaining automation

## Communication Protocol

Manager, accounts, and repositories communicate using a JSON-based protocol as defined in `docs/MANAGER_PROTOCOL.md`.

## File Locations

- `ACCOUNTS.json` - Main configuration file
- `PROJECT_MANAGER.md` - Instructions for PROJECT MANAGER AI
- `TASK_MANAGER.md` - Instructions for TASK MANAGER AI
- `VFSBOOT.md` - Auto-generated documentation file

## Integration with AI Instructions

The system uses specialized instructions for different AI sessions:

- **PROJECT_MANAGER.md**: Strategic planning and architectural decisions
- **TASK_MANAGER.md**: Task coordination and routine issue resolution
- Repository sessions follow instructions provided by the manager system

These files are loaded at startup and influence the behavior of the specialized AI sessions.