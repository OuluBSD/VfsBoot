#!/bin/bash

# qwen Account Client - Connect ACCOUNT to manager from remote computer
# Usage: ./account_client.sh --manager-host <host> --manager-port <port>

# Default values
MANAGER_HOST="localhost"
MANAGER_PORT="7778"
ACCOUNT_ID=""
HOSTNAME=$(hostname)
REPOSITORIES=()

# Function to display help
show_help() {
    echo "qwen Account Client - Connect to qwen manager from remote computer"
    echo ""
    echo "Usage: $0 --manager-host <host> --manager-port <port> [OPTIONS]"
    echo ""
    echo "Required Options:"
    echo "  --manager-host <host>   Manager host to connect to (default: localhost)"
    echo "  --manager-port <port>   Manager port to connect to (default: 7778)"
    echo ""
    echo "Optional Options:"
    echo "  --account-id <id>       Account ID to use (default: auto-generated)"
    echo "  --hostname <name>       Hostname to report to manager (default: $(hostname))"
    echo "  --repository <path>     Repository path to register (can be used multiple times)"
    echo "  --help                  Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 --manager-host 192.168.1.100 --manager-port 7778"
    echo "  $0 --manager-host example.com --repository /path/to/repo1 --repository /path/to/repo2"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --manager-host)
            MANAGER_HOST="$2"
            shift 2
            ;;
        --manager-port)
            MANAGER_PORT="$2"
            shift 2
            ;;
        --account-id)
            ACCOUNT_ID="$2"
            shift 2
            ;;
        --hostname)
            HOSTNAME="$2"
            shift 2
            ;;
        --repository)
            REPOSITORIES+=("$2")
            shift 2
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Validate required parameters
if [[ -z "$MANAGER_HOST" || -z "$MANAGER_PORT" ]]; then
    echo "Error: Both --manager-host and --manager-port are required"
    echo ""
    show_help
    exit 1
fi

# Generate account ID if not provided
if [[ -z "$ACCOUNT_ID" ]]; then
    # Generate a simple unique ID based on timestamp and hostname
    TIMESTAMP=$(date +%s)
    RANDOM_ID=$(openssl rand -hex 4 2>/dev/null || echo $((RANDOM % 10000)))
    ACCOUNT_ID="acc-${HOSTNAME}-${TIMESTAMP}-${RANDOM_ID}"
fi

echo "qwen Account Client"
echo "Connecting to manager: $MANAGER_HOST:$MANAGER_PORT"
echo "Account ID: $ACCOUNT_ID"
echo "Hostname: $HOSTNAME"
echo "Repositories: ${#REPOSITORIES[@]}"
if [ ${#REPOSITORIES[@]} -gt 0 ]; then
    for repo in "${REPOSITORIES[@]}"; do
        echo "  - $repo"
    done
fi
echo ""

# Establish connection to manager
echo "Establishing connection to manager at $MANAGER_HOST:$MANAGER_PORT..."
exec 3<>/dev/tcp/$MANAGER_HOST/$MANAGER_PORT

if [ $? -ne 0 ]; then
    echo "Error: Failed to connect to manager at $MANAGER_HOST:$MANAGER_PORT"
    exit 1
fi

echo "Connected to manager successfully!"

# Send initial connection message (JSON)
CONNECTION_MSG=$(cat <<EOF
{
  "type": "account_connection",
  "account_id": "$ACCOUNT_ID",
  "hostname": "$HOSTNAME",
  "timestamp": $(date +%s),
  "repositories": [
EOF
)

# Add repositories to the connection message
FIRST_REPO=true
for repo in "${REPOSITORIES[@]}"; do
    if [ "$FIRST_REPO" = true ]; then
        CONNECTION_MSG="$CONNECTION_MSG\n    {\"path\": \"$repo\", \"enabled\": true}"
        FIRST_REPO=false
    else
        CONNECTION_MSG="$CONNECTION_MSG,\n    {\"path\": \"$repo\", \"enabled\": true}"
    fi
done

CONNECTION_MSG="$CONNECTION_MSG\n  ]\n}"

# Send the connection message to the manager
printf "$CONNECTION_MSG\n" >&3

echo "Connection message sent to manager."

# Keep the connection alive and handle messages from the manager
while true; do
    # Read any incoming messages from the manager (with timeout)
    if IFS= read -r -t 10 message <&3; then
        if [ $? -eq 0 ]; then
            echo "Received from manager: $message"
            
            # Process manager commands here
            case "$message" in
                *"command"*)
                    # Parse command from manager (simplified)
                    command=$(echo "$message" | grep -o '"command":"[^"]*"' | cut -d'"' -f4)
                    if [ -n "$command" ]; then
                        echo "Processing manager command: $command"
                        
                        # Example: handle a command from the manager
                        case "$command" in
                            "ping")
                                # Respond to ping
                                response="{\"type\": \"ping_response\", \"account_id\": \"$ACCOUNT_ID\", \"timestamp\": $(date +%s)}"
                                printf "$response\n" >&3
                                ;;
                            "list_repos")
                                # Response with repository list
                                repo_list="{\"type\": \"repository_list\", \"account_id\": \"$ACCOUNT_ID\", \"repositories\": ["
                                first_repo=true
                                for repo in "${REPOSITORIES[@]}"; do
                                    if [ "$first_repo" = true ]; then
                                        repo_list="${repo_list}{\"path\": \"$repo\", \"enabled\": true, \"status\": \"active\"}"
                                        first_repo=false
                                    else
                                        repo_list="${repo_list}, {\"path\": \"$repo\", \"enabled\": true, \"status\": \"active\"}"
                                    fi
                                done
                                repo_list="${repo_list} ]}"
                                printf "$repo_list\n" >&3
                                ;;
                            *)
                                echo "Unknown command from manager: $command"
                                ;;
                        esac
                    fi
                    ;;
            esac
        fi
    else
        # Timeout occurred, send a heartbeat to keep the connection alive
        heartbeat="{\"type\": \"heartbeat\", \"account_id\": \"$ACCOUNT_ID\", \"timestamp\": $(date +%s)}"
        printf "$heartbeat\n" >&3
    fi
done

# Close the connection when exiting
exec 3<&-
exec 3>&-