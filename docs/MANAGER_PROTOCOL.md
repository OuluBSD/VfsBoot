# Manager-Account-Repo Communication Protocol

This document defines the JSON-based communication protocol between the qwen manager, accounts, and repository sessions.

## Overview

The manager mode uses a hierarchical communication model:

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

## Connection Protocol

### Account Connection

When an account connects to the manager, it sends an initial connection message:

```json
{
  "type": "account_connection",
  "account_id": "acc-12345",
  "hostname": "computer-a",
  "timestamp": 1234567890,
  "repositories": [
    {
      "path": "/path/to/repo",
      "enabled": true
    }
  ]
}
```

### Manager Response

The manager responds with configuration and status:

```json
{
  "type": "connection_accepted",
  "manager_id": "mgr-main",
  "accounts_config": {
    "max_concurrent_repos": 3
  },
  "commands": []
}
```

## Message Types

### 1. Task Assignment

The manager sends task specifications to accounts:

```json
{
  "type": "task_assignment",
  "task_id": "task-123",
  "repository_id": "repo-456",
  "task_specification": {
    "title": "Implement feature X",
    "description": "Detailed description of the work to be done",
    "requirements": ["req-1", "req-2"],
    "deadline": 1234567890
  },
  "priority": "high"  // low, medium, high, critical
}
```

### 2. Status Updates

Accounts send status updates about repositories and tasks:

```json
{
  "type": "status_update",
  "account_id": "acc-12345",
  "repository_id": "repo-456",
  "status": {
    "state": "working",  // idle, working, blocked, error, complete
    "progress": 0.75,    // 0.0 to 1.0
    "active_sessions": ["worker-1", "manager-2"],
    "last_activity": 1234567890,
    "error_message": "optional error message if state is error"
  }
}
```

### 3. Escalation Messages

When WORKER sessions fail, they escalate to MANAGER:

```json
{
  "type": "escalation",
  "escalation_id": "esc-789",
  "repository_id": "repo-456",
  "source_session": "worker-1",
  "target_session": "manager-2", // which manager to escalate to
  "problem_description": "Detailed description of the problem",
  "context": {
    "failed_attempts": 3,
    "last_error": "Error message from the failed attempt",
    "work_done_so_far": "Summary of completed work"
  }
}
```

### 4. Command Messages

Commands sent from manager to account/repository:

```json
{
  "type": "command",
  "command": "enable_repo", // enable_repo, disable_repo, run_tests, stop_session, restart_session
  "repository_id": "repo-456",
  "parameters": {
    "reason": "optional reason for the command"
  }
}
```

### 5. Heartbeat Messages

Regular heartbeat to maintain connection:

```json
{
  "type": "heartbeat",
  "account_id": "acc-12345",
  "timestamp": 1234567890
}
```

### 6. Result Messages

Results from task completion:

```json
{
  "type": "task_result",
  "task_id": "task-123",
  "repository_id": "repo-456",
  "result": {
    "status": "success",  // success, partial_success, failure
    "output": "Description of what was accomplished",
    "files_changed": ["/path/to/file1", "/path/to/file2"],
    "test_results": {
      "passed": 15,
      "failed": 2,
      "total": 17
    }
  }
}
```

## Error Handling

### Connection Errors

- If heartbeat is not received for 30 seconds, connection is considered lost
- Manager attempts to reconnect for up to 5 minutes before marking account as offline
- When reconnecting, account should report current status of all managed repositories

### Task Errors

- If a task fails repeatedly, it's escalated to higher-level manager
- After 3 failed attempts by WORKER, escalate to REPO MANAGER
- After 3 failed attempts by REPO MANAGER, escalate to TASK MANAGER
- After 3 failed attempts by TASK MANAGER, escalate to PROJECT MANAGER

## Session Management

### Session Lifecycle

1. Manager sends task_assignment to account
2. Account creates WORKER session for implementation
3. If WORKER fails, optionally escalate to MANAGER session
4. Sessions report status via status_update messages
5. Upon completion, session sends task_result
6. Session is closed if inactive for 10 minutes

### Auto-scaling

- Accounts automatically start/stop repository sessions based on assigned tasks
- Enforce max_concurrent_repos limit from ACCOUNTS.json
- Sessions are kept alive while tasks are in progress
- Idle sessions are closed after timeout

## Implementation Notes

1. All messages are JSON formatted
2. Message size should be kept under 1MB
3. Implement retry logic with exponential backoff for failed messages
4. All timestamps are Unix timestamps (seconds since epoch)
5. Use UTF-8 encoding for all text fields