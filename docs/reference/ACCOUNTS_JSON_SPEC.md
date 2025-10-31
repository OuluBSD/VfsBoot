# ACCOUNTS.json Schema Specification

This document defines the schema for the ACCOUNTS.json configuration file that controls the manager mode in qwen.

## Root Structure

```json
{
  "accounts": [AccountObject]
}
```

## Account Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | string | Yes | Unique identifier for the account |
| `hostname` | string | Yes | Hostname or IP address of the computer running the account |
| `enabled` | boolean | Yes | Whether the account is enabled for connections |
| `max_concurrent_repos` | integer | No | Maximum number of repositories that can run simultaneously (default: 3) |
| `repositories` | [RepositoryObject] | Yes | Array of repositories managed by this account |

## Repository Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | string | Yes | Unique identifier for the repository |
| `url` | string | Yes | Git URL for the repository |
| `local_path` | string | Yes | Local path where the repository is cloned |
| `enabled` | boolean | Yes | Whether the repository is enabled for management |
| `worker_model` | string | No | AI model to use for REPO_WORKER sessions (default: "qwen-auth") |
| `manager_model` | string | No | AI model to use for REPO_MANAGER sessions (default: "qwen-openai") |

## Example Configuration

```json
{
  "accounts": [
    {
      "id": "account-1",
      "hostname": "computer-a",
      "enabled": true,
      "max_concurrent_repos": 3,
      "repositories": [
        {
          "id": "repo-1",
          "url": "https://github.com/user/repo1.git",
          "local_path": "/path/to/local/clone",
          "enabled": true,
          "worker_model": "qwen-auth",
          "manager_model": "qwen-openai"
        },
        {
          "id": "repo-2",
          "url": "https://github.com/user/repo2.git",
          "local_path": "/path/to/local/clone2",
          "enabled": false,
          "worker_model": "qwen-auth",
          "manager_model": "qwen-openai"
        }
      ]
    },
    {
      "id": "account-2",
      "hostname": "computer-b",
      "enabled": true,
      "max_concurrent_repos": 2,
      "repositories": [
        {
          "id": "repo-3",
          "url": "https://github.com/user/repo3.git",
          "local_path": "/path/to/local/clone3",
          "enabled": true,
          "worker_model": "qwen-auth",
          "manager_model": "qwen-openai"
        }
      ]
    }
  ]
}
```

## Validation Rules

1. Account IDs must be unique across all accounts
2. Repository IDs must be unique within each account
3. Hostname must be a valid hostname or IP address
4. max_concurrent_repos must be a positive integer
5. Repository URLs must be valid Git URLs
6. Local paths must be valid filesystem paths
7. Models must be valid qwen models
8. All required fields must be present

## Versioning

This schema uses semantic versioning. The version of the schema can be found in the schema definition or accompanying documentation. The current version is 1.0.0.