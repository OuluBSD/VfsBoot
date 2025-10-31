# qwen Manager Mode Tutorial

This tutorial will guide you through setting up and using the qwen Manager Mode for multi-repository AI project management.

## Prerequisites

Before starting, ensure you have:

1. qwen-code properly installed and running
2. Network access for TCP connections between manager and accounts
3. Git repositories you want to manage
4. Appropriate API keys for your AI models (qwen-openai, qwen-auth)

## Step 1: Create Management Repository

First, create a management repository to store your configurations:

```bash
mkdir my-management-repo
cd my-management-repo
git init
```

This repository will contain your configuration files and serve as the central hub for your project management.

## Step 2: Configure ACCOUNTS.json

Create an `ACCOUNTS.json` file in your management repository with the following structure:

```json
{
  "accounts": [
    {
      "id": "development-machine",
      "hostname": "localhost",  // or IP address of the account machine
      "enabled": true,
      "max_concurrent_repos": 3,
      "repositories": [
        {
          "id": "my-project",
          "url": "https://github.com/username/my-project.git",
          "local_path": "/path/to/local/clone",
          "enabled": true,
          "worker_model": "qwen-auth",
          "manager_model": "qwen-openai"
        }
      ]
    }
  ]
}
```

### Configuration Explained

- `id`: A unique identifier for this account
- `hostname`: The address where the account can be reached (IP or hostname)
- `enabled`: Whether this account is active (true/false)
- `max_concurrent_repos`: Maximum number of repositories to manage simultaneously
- `repositories[].id`: Unique ID for the repository
- `repositories[].url`: Git URL for the repository
- `repositories[].local_path`: Local path to clone the repository
- `repositories[].enabled`: Whether to manage this repository
- `repositories[].worker_model`: AI model for routine work
- `repositories[].manager_model`: AI model for complex tasks

## Step 3: Set Up AI Instructions

Create specialized instruction files for your AI managers:

### PROJECT_MANAGER.md
Create a file called `PROJECT_MANAGER.md` with strategic planning instructions:

```markdown
# PROJECT MANAGER Role and Responsibilities

You are a PROJECT MANAGER AI with the following specific role and responsibilities:

## Primary Role
- You are the primary decision maker for high-level project strategy and architecture
- You use the qwen-openai model for complex reasoning and planning
- You are expensive but high quality - use your capabilities for strategic decisions
- You maintain overall project vision and ensures alignment with business goals

[Add more detailed instructions for strategic planning tasks]
```

### TASK_MANAGER.md
Create a file called `TASK_MANAGER.md` with task coordination instructions:

```markdown
# TASK MANAGER Role and Responsibilities

You are a TASK MANAGER AI with the following specific role and responsibilities:

## Primary Role
- You manage specific project tasks and coordinate their execution
- You use the qwen-auth model for regular quality work at a lower cost
- You act as a coordinator between strategic decisions from PROJECT MANAGER and implementation work
- You maintain task-level visibility and ensure progress toward objectives

[Add more detailed instructions for task coordination]
```

## Step 4: Start the Manager

Navigate to your management repository and start the manager mode:

```bash
cd /path/to/my-management-repo
./vfsh
vfsh> qwen --manager
# or
vfsh> qwen -m
```

The manager will:
1. Load your `ACCOUNTS.json` configuration
2. Create PROJECT MANAGER and TASK MANAGER sessions
3. Start the TCP server for account connections
4. Display the session list and chat interface

## Step 5: Connect an Account

On the machine that will manage repositories, start the account client:

```bash
cd /path/to/VfsBoot
./scripts/account_client.sh --manager-host <manager-ip> --manager-port 7778
```

Replace `<manager-ip>` with the IP address of the machine running the manager.

## Step 6: Configure Repository Access

Ensure the account machine has access to the repositories specified in your `ACCOUNTS.json`:

1. Clone the repositories to the specified `local_path`
2. Verify git access (SSH keys, HTTPS credentials)
3. Ensure the paths exist and are writable

## Step 7: Use the Manager Interface

Once connected, you'll see the manager interface:

### Session List (Top Panel)
Shows all sessions with color coding:
- Cyan: PROJECT MANAGER and TASK MANAGER
- Yellow: ACCOUNT sessions
- Green: REPO sessions

### Chat/Command Interface (Bottom Panel)
When you select different sessions, this area changes context:

#### ACCOUNT Sessions
Support these commands without the `/` prefix:
- `list` - Show all repositories for this account
- `enable <repo_id>` - Enable a specific repository
- `disable <repo_id>` - Disable a specific repository
- `status <repo_id>` - Check repository status

#### REPO Sessions
Can be controlled manually or left in automatic mode.
- Use `/auto` to return to automatic workflow after manual intervention

## Step 8: Monitor Automation Workflows

The manager mode includes automated workflows:

### Failure Escalation
- If a REPO WORKER fails 3 times consecutively, the task escalates to the REPO MANAGER
- This ensures difficult tasks get more advanced AI attention

### Review Cycles
- After 3 commits from a REPO WORKER, the REPO MANAGER triggers a review
- This maintains code quality through regular oversight

### Manual Override
- At any time, you can manually intervene in repository sessions
- Use the `/auto` command to return to automatic operation

## Step 9: Managing with Commands

### Switching Focus
- Press **TAB** to switch between the session list and the input area
- Use arrow keys to navigate the session list when focused
- Press **ENTER** to make a session active in the chat view

### Session Management
- Select a session from the top list to view its chat history and send commands
- Different session types have context-appropriate commands
- The status bar shows current model, session ID, and permission mode

## Troubleshooting

### Connection Issues
- Verify the manager is running and listening on the correct port
- Check firewall settings between manager and account machines
- Ensure the hostname/IP in ACCOUNTS.json matches the actual address

### Repository Access
- Verify git credentials and SSH keys are properly configured
- Check that the local_path exists and has proper permissions
- Ensure the repository is properly cloned

### AI Model Issues
- Verify your API keys are configured correctly
- Check that the specified models (qwen-openai, qwen-auth) are available
- Review any authentication setup that may be required

## Next Steps

Once your manager mode is set up:

1. Experiment with the automation workflows
2. Fine-tune your AI instructions in PROJECT_MANAGER.md and TASK_MANAGER.md
3. Add more accounts and repositories to your ACCOUNTS.json
4. Monitor the effectiveness of automatic escalations and reviews
5. Adjust max_concurrent_repos based on your resources and needs

Your qwen Manager Mode is now ready to automate and coordinate multi-repository AI development workflows!