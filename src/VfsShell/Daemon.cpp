#include "VfsShell.h"


// ====== Daemon Server Mode ======

void run_daemon_server(int port, Vfs&, std::shared_ptr<Env>, WorkingDirectory&) {
    TRACE_FN("port=", port);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        throw std::runtime_error("daemon: failed to create socket");
    }

    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        close(server_fd);
        throw std::runtime_error("daemon: setsockopt failed");
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        close(server_fd);
        throw std::runtime_error("daemon: bind failed on port " + std::to_string(port));
    }

    if(listen(server_fd, 5) < 0){
        close(server_fd);
        throw std::runtime_error("daemon: listen failed");
    }

    std::cout << "daemon: listening on port " << port << "\n";
    std::cout << "daemon: ready to accept VFS remote mount connections\n";

    while(true){
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

        if(client_fd < 0){
            std::cerr << "daemon: accept failed\n";
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "daemon: connection from " << client_ip << ":" << ntohs(client_addr.sin_port) << "\n";

        // Handle client connection in separate thread
        std::thread([client_fd](){
            auto handle_request = [](const std::string& request) -> std::string {
                try {
                    // Parse: EXEC <command>\n
                    if(request.substr(0, 5) != "EXEC "){
                        return "ERR invalid command format\n";
                    }

                    std::string command = request.substr(5);
                    if(!command.empty() && command.back() == '\n'){
                        command.pop_back();
                    }

                    // Execute command as shell command and capture output
                    FILE* pipe = popen(command.c_str(), "r");
                    if(!pipe){
                        return "ERR failed to execute command\n";
                    }

                    std::string output;
                    char buf[4096];
                    while(fgets(buf, sizeof(buf), pipe) != nullptr){
                        output += buf;
                    }

                    int status = pclose(pipe);
                    if(status != 0){
                        return "ERR command failed with status " + std::to_string(status) + "\n";
                    }

                    return "OK " + output + "\n";
                } catch(const std::exception& e){
                    return "ERR " + std::string(e.what()) + "\n";
                } catch(...){
                    return "ERR internal error\n";
                }
            };

            try {
                while(true){
                    char buf[4096];
                    ssize_t n = recv(client_fd, buf, sizeof(buf)-1, 0);
                    if(n <= 0) break;

                    buf[n] = '\0';
                    std::string request(buf);
                    std::string response = handle_request(request);

                    send(client_fd, response.c_str(), response.size(), 0);
                }
            } catch(...){
                // Connection closed or error
            }

            close(client_fd);
        }).detach();
    }

    close(server_fd);
}

