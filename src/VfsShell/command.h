#ifndef _VfsShell_command_h_
#define _VfsShell_command_h_


extern RulePatchStaging* G_PATCH_STAGING;
extern FeedbackLoop* G_FEEDBACK_LOOP;

size_t select_overlay(const Vfs& vfs, const WorkingDirectory& cwd, const std::vector<size_t>& overlays);
std::string overlay_suffix(const Vfs& vfs, const std::vector<size_t>& overlays, size_t primary);

void run_daemon_server(int port, Vfs&, std::shared_ptr<Env>, WorkingDirectory&);

struct CommandResult {
    bool success = true;
    bool exit_requested = false;
    std::string output;
};

extern MetricsCollector* G_METRICS_COLLECTOR;



struct CommandInvocation {
    std::string name;
    std::vector<std::string> args;
};

struct CommandPipeline {
    std::vector<CommandInvocation> commands;
    std::string output_redirect;  // file path for > or >>
    bool redirect_append = false; // true for >>, false for >
};

struct CommandChainEntry {
    std::string logical; // "", "&&", "||"
    CommandPipeline pipeline;
};

struct ScopedCoutCapture {
    std::ostringstream buffer;
    std::streambuf* old = nullptr;
    ScopedCoutCapture(){
        old = std::cout.rdbuf(buffer.rdbuf());
    }
    ~ScopedCoutCapture(){
        std::cout.rdbuf(old);
    }
    std::string str() const { return buffer.str(); }
};

struct RawTerminalMode {
    termios original{};
    bool active = false;

    RawTerminalMode(){
        if(::isatty(STDIN_FILENO) != 1) return;
        if(tcgetattr(STDIN_FILENO, &original) != 0) return;
        termios raw = original;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
        raw.c_oflag &= static_cast<tcflag_t>(~OPOST);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0){
            active = true;
        }
    }

    ~RawTerminalMode(){
        if(active){
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
        }
    }

    bool ok() const { return active; }
};


bool terminal_available();
void redraw_prompt_line(const std::string& prompt, const std::string& buffer, size_t cursor);
std::string complete_input(Vfs& vfs, const std::string& buffer, size_t cursor, const std::string& cwd_path, bool& show_list);
void load_history(std::vector<std::string>& history);
bool read_line_with_history(Vfs& vfs, const std::string& prompt, std::string& out, const std::vector<std::string>& history, const std::string& cwd_path);
std::vector<std::string> tokenize_command_line(const std::string& line);
void save_history(const std::vector<std::string>& history);
std::vector<CommandChainEntry> parse_command_chain(const std::vector<std::string>& tokens);

#endif
