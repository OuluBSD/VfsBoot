# VFSBOOT - qwen Manager Documentation

This document provides an overview of the qwen Manager Mode and its components.

## Overview

The qwen Manager Mode enables hierarchical multi-repository AI project management with the following components:

- **PROJECT MANAGER**: Expensive, high-quality AI for strategic decisions (qwen-openai)
- **TASK MANAGER**: Regular quality AI for task coordination (qwen-auth) 
- **ACCOUNTS**: Remote computers that connect to the manager
- **REPOSITORIES**: Individual project repositories managed by worker/manager pairs

## Configuration

The system is configured using `ACCOUNTS.json` which defines accounts, repositories, and their properties.

### ACCOUNTS.json Schema

The configuration file follows this schema:

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

For more details about the schema, see [docs/ACCOUNTS_JSON_SPEC.md](docs/ACCOUNTS_JSON_SPEC.md).

## Communication Protocol

The manager, accounts, and repositories communicate using a JSON-based protocol. For the detailed specification, see [docs/MANAGER_PROTOCOL.md](docs/MANAGER_PROTOCOL.md).

## AI Role Definitions

This system uses specialized AI roles with specific responsibilities:

- [PROJECT_MANAGER.md](PROJECT_MANAGER.md) - Instructions for PROJECT MANAGER AI (qwen-openai)
- [TASK_MANAGER.md](TASK_MANAGER.md) - Instructions for TASK MANAGER AI (qwen-auth)

## File Locations

- `ACCOUNTS.json` - Main configuration file for defining accounts and repositories
- `PROJECT_MANAGER.md` - AI instructions for project-level management
- `TASK_MANAGER.md` - AI instructions for task-level coordination  
- `docs/ACCOUNTS_JSON_SPEC.md` - Schema specification for configuration
- `docs/MANAGER_PROTOCOL.md` - Communication protocol specification

## Usage

To start the manager mode:

```bash
qwen --manager
# or
qwen -m
```

This will initialize the manager, load the account configurations, start the TCP server for account connections, and provide the UI for managing the multi-repository setup.
