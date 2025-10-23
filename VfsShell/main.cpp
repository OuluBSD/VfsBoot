#include "VfsShell.h"
#include "upp_workspace_build.h"
#include "registry.h"
#include "cmd_qwen.h"
#include <filesystem>
#include <unistd.h>

WINDOW* stdscr;

// Global storage for loaded startup assemblies
std::vector<std::shared_ptr<UppAssembly>> g_startup_assemblies;

// Global reference to the currently active assembly
std::shared_ptr<UppAssembly> g_current_assembly = nullptr;

// Global U++ builder registry
UppBuilderRegistry g_upp_builder_registry;

// Global registry instance
Registry g_registry;

// Global variable to track the active file in the workspace
std::string g_active_file_path = "";

namespace {

bool has_bm_extension(const std::string& name) {
    if(name.size() < 3) return false;
    std::string suffix = name.substr(name.size() - 3);
    std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return suffix == ".bm";
}

void dump_builder(const UppBuildMethod& builder) {
    std::cout << "Builder: " << builder.id;
    if(!builder.builder_type.empty() && builder.builder_type != builder.id) {
        std::cout << " (type: " << builder.builder_type << ")";
    }
    std::cout << "\n";
    if(!builder.source_path.empty()) {
        std::cout << "Source: " << builder.source_path << "\n";
    }
    for(const auto& key : builder.keys()) {
        auto it = builder.properties.find(key);
        if(it == builder.properties.end()) continue;
        std::cout << "  " << key << " = \"" << it->second << "\"\n";
    }
}

} // namespace


void help(){
    TRACE_FN();
    std::cout <<
R"(Commands:
  pwd
  cd [path]
  ls [path]
  tree [path]
  mkdir <path>
  touch <path>
  rm <path>
  mv <src> <dst>
  link <src> <dst>
  export <vfs> <host>
  cat [paths...] (tai stdin jos ei polkuja)
  grep [-i] <pattern> [path]
  rg [-i] <pattern> [path]
  head [-n N] [path]
  tail [-n N] [path]
  uniq [path]
  count [path]
  history [-a | -n N]
  random [min [max]]
  true / false
  echo <path> <data...>
  edit [file-path]           (full-screen text editor - also available as 'ee')
  parse <src-file> <dst-ast>
  eval <ast-path>
  putkita komentoja: a | b | c, a && b, a || b
  # AI
  discuss <message...>     (natural language → plans → implementation)
  discuss.session new [name] | end
  ai <prompt...>
  ai.brief <key> [extra...]
  tools
  overlay.list
  overlay.mount <name> <file>
  overlay.save <name> <file>
  overlay.unmount <name>
  overlay.policy [manual|oldest|newest]
  overlay.use <name>
  solution.save [file]
  # Filesystem mounts
  mount <host-path> <vfs-path>
  mount.lib <lib-path> <vfs-path>
  mount.remote <host> <port> <remote-vfs-path> <local-vfs-path>
  mount.list
  mount.allow
  mount.disallow
  unmount <vfs-path>
  # Tags (metadata for nodes)
  tag.add <vfs-path> <tag-name> [tag-name...]
  tag.remove <vfs-path> <tag-name> [tag-name...]
  tag.list [vfs-path]
  tag.clear <vfs-path>
  tag.has <vfs-path> <tag-name>
  # Registry (Windows Registry-like key-value store)
  reg.set <key> <value>        (set registry key value)
  reg.get <key>                (get registry key value)
  reg.list [path]              (list registry keys and values)
  reg.rm <key>                 (remove registry key or value)
  # Logic System (tag theorem proving and inference)
  logic.init                        (load hardcoded implication rules)
  logic.infer <tag> [tag...]        (infer tags via forward chaining)
  logic.check <tag> [tag...]        (check consistency, detect conflicts)
  logic.explain <target> <source...> (explain why target inferred from sources)
  logic.listrules                   (list all loaded implication rules)
  logic.sat <tag> [tag...]          (check if formula is satisfiable)
  # Tag Mining (extract user's mental model)
  tag.mine.start <tag> [tag...]     (start mining session with initial tags)
  tag.mine.feedback <tag> yes|no    (provide feedback on inferred tags)
  tag.mine.status                   (show current mining session status)
  # Planner (hierarchical planning system)
  plan.create <path> <type> [content]
  plan.goto <path>
  plan.forward
  plan.backward
  plan.context.add <vfs-path> [vfs-path...]
  plan.context.remove <vfs-path> [vfs-path...]
  plan.context.clear
  plan.context.list
  plan.status
  plan.discuss [message...]     (interactive AI discussion about current plan)
  plan.answer <yes|no|explain> [reason...]  (answer AI questions)
  plan.hypothesis [type]        (generate hypothesis for current plan)
  plan.jobs.add <jobs-path> <description> [priority] [assignee]
  plan.jobs.complete <jobs-path> <index>
  plan.verify [path]                     (check tag consistency for plan node)
  plan.tags.infer [path]                 (show complete inferred tag set for plan)
  plan.tags.check [path]                 (verify no tag conflicts in plan)
  plan.validate [path]                   (recursively validate entire plan subtree)
  plan.save [file]
  # Action Planner (context building & testing)
  context.build [max_tokens]
  context.build.adv [max_tokens] [--deps] [--dedup] [--summary=N] [--hierarchical] [--adaptive]
  context.filter.tag <tag-name> [any|all|none]
  context.filter.path <prefix-or-pattern>
  tree.adv [path] [--no-box] [--sizes] [--tags] [--colors] [--kind] [--sort] [--depth=N] [--filter=pattern]
  test.planner
  # Hypothesis Testing (5 progressive complexity levels)
  test.hypothesis                              (run all 5 levels)
  hypothesis.test <level> <goal> [desc]        (test custom hypothesis, level 1-5)
  hypothesis.query <target> [path]             (Level 1: find pattern)
  hypothesis.errorhandling <func> [style]      (Level 2: add error handling)
  hypothesis.duplicates [path] [min_lines]     (Level 3: find duplicate code)
  hypothesis.logging [path]                    (Level 4: plan logging instrumentation)
  hypothesis.pattern <pattern> [path]          (Level 5: evaluate architecture pattern)
  # Feedback Pipeline (automated rule evolution)
  feedback.metrics.show [top_n]                (show metrics history and top rules)
  feedback.metrics.save [path]                 (save metrics to VFS file)
  feedback.patches.list                        (list pending rule patches)
  feedback.patches.apply [index|all]           (apply pending patches)
  feedback.patches.reject [index|all]          (reject pending patches)
  feedback.patches.save [path]                 (save patches to VFS file)
  feedback.cycle [--auto-apply] [--min-evidence=N]  (run full feedback cycle)
  feedback.review                              (interactive patch review)
  # C++ builder
  cpp.tu <ast-path>
  cpp.include <tu-path> <header> [angled0/1]
  cpp.func <tu-path> <name> <ret>
  cpp.param <fn-path> <type> <name>
  cpp.print <scope-path> <text>
  cpp.vardecl <scope-path> <type> <name> [init]
  cpp.expr <scope-path> <expression>
  cpp.stmt <scope-path> <raw>
  cpp.return <scope-path> [expression]
  cpp.returni <scope-path> <int>
  cpp.rangefor <scope-path> <loop-name> <decl> | <range>
  cpp.dump <tu-path> <vfs-file-path>
  # Qwen AI Assistant
  qwen [options]                                  (interactive AI assistant)
  qwen --attach <id>                              (attach to existing session)
  qwen --list-sessions                            (list all sessions)
  # Build automation
  make [target] [-f makefile] [-v|--verbose]  (minimal GNU make subset)
  sample.run [--keep] [--trace]               (build, compile, and run demo C++ program)
  # U++ builder support
  upp.builder.load <directory-path> [-H]      (load all .bm files from directory, -H treats path as OS filesystem path)
  upp.builder.add <bm-file-path> [-H]         (load a single .bm build method file)
  upp.builder.list                            (list loaded build methods)
  upp.builder.active.set <builder-name>       (set the active build method)
  upp.builder.get <key>                       (show a key from the active build method)
  upp.builder.set <key> <value>               (update a key in the active build method)
  upp.builder.dump <builder-name>             (dump all keys for a build method)
  upp.builder.active.dump                     (dump keys for the active build method)
  # U++ startup support
  upp.startup.load <directory-path> [-H]      (load all .var files from directory, -H treats path as OS filesystem path)
  upp.startup.list                            (list all loaded startup assemblies)
  upp.startup.open <name> [-v]                (load a named startup assembly, -v for verbose)
  # U++ assembly support
  upp.asm.load <var-file-path> [-H]           (load U++ assembly file, -H treats path as OS filesystem path)
  upp.asm.create <name> <output-path>         (create new U++ assembly)
  upp.asm.list [-v]                           (list packages in current assembly, -v for all directories)
  upp.asm.scan <directory-path>               (scan directory for U++ packages with .upp files)
  upp.asm.load.host <host-var-file>           (mount host dir and load .var file from OS filesystem)
  upp.asm.refresh [-v]                        (refresh all packages in active assembly, -v for verbose)
  # U++ workspace support
  upp.wksp.open <pkg-name> [-v]               (open a package from the list as workspace)
  upp.wksp.open -p <path> [-v]                (open a U++ package as workspace from path, -v for verbose)
  upp.wksp.close                              (close current workspace)
  upp.wksp.pkg.list                           (list packages in current workspace)
  upp.wksp.pkg.set <package-name>             (set active package in workspace)
  upp.wksp.file.list                          (list files in active package)
  upp.wksp.file.set <file-path>               (set active file in editor)
  upp.wksp.build [options]                    (build active workspace package and dependencies)
  upp.gui                                     (launch U++ assembly IDE GUI)
  # libclang C++ AST parsing
  parse.file <filepath> [vfs-target-path]     (parse C++ file with libclang)
  parse.dump [vfs-path]                       (dump parsed AST tree)
  parse.generate <ast-path> <output-path>     (generate C++ code from AST)
Notes:
  - Polut voivat olla suhteellisia nykyiseen VFS-hakemistoon (cd).
  - ./codex <skripti> suorittaa komennot tiedostosta ilman REPL-kehotetta.
  - ./codex <skripti> - suorittaa skriptin ja palaa interaktiiviseen tilaan.
  - F3 tallentaa aktiivisen solutionin (sama kuin solution.save).
  - ai.brief lukee promptit snippets/-hakemistosta (CODEX_SNIPPET_DIR ylikirjoittaa polun).
  - OPENAI_API_KEY pakollinen 'ai' komentoon OpenAI-tilassa. OPENAI_MODEL (oletus gpt-4o-mini), OPENAI_BASE_URL (oletus https://api.openai.com/v1).
  - Llama-palvelin: LLAMA_BASE_URL / LLAMA_SERVER (oletus http://192.168.1.169:8080), LLAMA_MODEL (oletus coder), CODEX_AI_PROVIDER=llama pakottaa käyttöön.
)"<<std::endl;
}


int qwen_client_test(int argc, char* argv[]);
int qwen_integration_test(int argc, char** argv);
int qwen_echo_server();
int qwen_protocol_tests();
int qwen_state_tests();

#ifndef CODEX_NO_MAIN
int main(int argc, char** argv){
	using namespace i18n;
	
    TRACE_FN();
    using std::string; using std::shared_ptr;
    std::ios::sync_with_stdio(false); std::cin.tie(nullptr);
    i18n::init();
    snippets::initialize(argc > 0 ? argv[0] : nullptr);

    auto usage = [&](const std::string& msg){
        std::cerr << msg << "\n";
        return 1;
    };

    const string usage_text = string("usage: ") + argv[0] + " [--solution <pkg|asm>] [--daemon <port>] [--web-server] [--port <port>] [--quiet] [script [-]]";

    std::string script_path;
    std::string solution_arg;
    bool fallback_after_script = false;
    int daemon_port = -1;
    bool web_server_mode = false;
    int web_server_port = 8080;  // Default port
    bool quiet_mode = false;

    auto looks_like_solution_hint = [](const std::string& arg){
        return is_solution_file(std::filesystem::path(arg));
    };

    for(int i = 1; i < argc; ++i){
        std::string arg = argv[i];
        if(arg == "--qwen-client-test")			return qwen_client_test(argc, argv);
        if(arg == "--qwen-integration-test")	return qwen_integration_test(argc, argv);
        if(arg == "--qwen-echo-server")			return qwen_echo_server();
        if(arg == "--qwen-protocol-tests")		return qwen_protocol_tests();
        if(arg == "--qwen-state-tests")			return qwen_state_tests();
        
        if(arg == "--solution" || arg == "-S"){
            if(i + 1 >= argc) return usage("--solution requires a file path");
            solution_arg = argv[++i];
            continue;
        }
        if(arg == "--daemon" || arg == "-d"){
            if(i + 1 >= argc) return usage("--daemon requires a port number");
            daemon_port = std::stoi(argv[++i]);
            continue;
        }
        if(arg == "--web-server" || arg == "-w"){
            web_server_mode = true;
            continue;
        }
        if(arg == "--port" || arg == "-p"){
            if(i + 1 >= argc) return usage("--port requires a port number");
            web_server_port = std::stoi(argv[++i]);
            continue;
        }
        if(arg == "--quiet" || arg == "-q"){
            quiet_mode = true;
            continue;
        }
        if(arg == "--script"){
            if(i + 1 >= argc) return usage("--script requires a file path");
            script_path = argv[++i];
            quiet_mode = true;  // Auto-enable quiet mode for scripts
            if(i + 1 < argc && std::string(argv[i + 1]) == "-"){
                fallback_after_script = true;
                ++i;
            }
            continue;
        }
        if(arg == "-"){
            if(script_path.empty()) return usage("'-' requires a preceding script path");
            fallback_after_script = true;
            continue;
        }
        if(solution_arg.empty() && looks_like_solution_hint(arg)){
            solution_arg = arg;
            continue;
        }
        if(script_path.empty()){
            script_path = arg;
            quiet_mode = true;  // Auto-enable quiet mode for script files
            continue;
        }
        return usage(usage_text);
    }

    // Enable quiet mode if running a script
    if(quiet_mode) {
        i18n::set_english_only();
    }

    bool interactive = script_path.empty();
    bool script_active = !interactive;
    std::unique_ptr<std::ifstream> scriptStream;
    std::istream* input = &std::cin;

    if(!script_path.empty()){
        scriptStream = std::make_unique<std::ifstream>(script_path);
        if(!*scriptStream){
            std::cerr << "failed to open script '" << script_path << "'\n";
            return 1;
        }
        input = scriptStream.get();
    }

    Vfs vfs; auto env = std::make_shared<Env>(); install_builtins(env);
    vfs.mkdir("/src"); vfs.mkdir("/ast"); vfs.mkdir("/env"); vfs.mkdir("/astcpp"); vfs.mkdir("/cpp"); vfs.mkdir("/plan");
    
    // Initialize registry
    g_registry.integrateWithVFS(vfs);

    // Initialize feedback pipeline
    MetricsCollector metrics_collector;
    RulePatchStaging patch_staging(&vfs.logic_engine);
    FeedbackLoop feedback_loop(metrics_collector, patch_staging, vfs.logic_engine, vfs.tag_registry);
    G_METRICS_COLLECTOR = &metrics_collector;
    G_PATCH_STAGING = &patch_staging;
    G_FEEDBACK_LOOP = &feedback_loop;

    WorkingDirectory cwd;
    update_directory_context(vfs, cwd, cwd.path);
    PlannerContext planner;
    planner.current_path = "/";
    DiscussSession discuss;

    // Auto-load .vfs file if present
    try{
        if(auto vfs_path = auto_detect_vfs_path()){
            auto abs_vfs_path = std::filesystem::absolute(*vfs_path);
            auto title = abs_vfs_path.parent_path().filename().string();
            if(title.empty()) title = "autoload";
            auto overlay_name = make_unique_overlay_name(vfs, title);
            mount_overlay_from_file(vfs, overlay_name, abs_vfs_path.string());
            std::cout << "auto-loaded " << abs_vfs_path.filename().string() << " as overlay '" << overlay_name << "'\n";
            maybe_extend_context(vfs, cwd);
        }
    } catch(const std::exception& e){
        std::cout << "note: auto-load .vfs failed: " << e.what() << "\n";
    }

    SolutionContext solution;
    std::optional<std::filesystem::path> solution_path_fs;
    try{
        if(!solution_arg.empty()){
            std::filesystem::path p = solution_arg;
            if(p.is_relative()) p = std::filesystem::absolute(p);
            solution_path_fs = p;
        } else if(auto auto_path = auto_detect_solution_path()){
            solution_path_fs = std::filesystem::absolute(*auto_path);
        }
    } catch(const std::exception& e){
        std::cout << "note: unable to resolve solution path: " << e.what() << "\n";
    }
    bool solution_loaded = false;
    if(solution_path_fs){
        if(!is_solution_file(*solution_path_fs)){
            std::cout << "note: '" << solution_path_fs->string() << "' does not use expected "
                      << kPackageExtension << " or " << kAssemblyExtension << " extension\n";
        }
        solution_loaded = load_solution_from_file(vfs, cwd, solution, *solution_path_fs, solution_arg.empty());
    }
    if(!solution_loaded){
        g_on_save_shortcut = nullptr;
    }

    // Auto-load plan.vfs if present (planner state persistence)
    try{
        std::filesystem::path plan_path = "plan.vfs";
        if(std::filesystem::exists(plan_path)){
            auto abs_plan_path = std::filesystem::absolute(plan_path);
            mount_overlay_from_file(vfs, "plan", abs_plan_path.string());
            std::cout << "auto-loaded plan.vfs into /plan tree\n";
            // Initialize planner to /plan if it exists
            if(auto plan_root = vfs.tryResolveForOverlay("/plan", 0)){
                if(plan_root->isDir()){
                    planner.current_path = "/plan";
                }
            }
        }
    } catch(const std::exception& e){
        std::cout << "note: auto-load plan.vfs failed: " << e.what() << "\n";
    }

    // Auto-mount directories from UPP environment variable
    try {
        if(const char* upp_env = std::getenv("UPP")) {
            std::string upp_paths = std::string(upp_env);
            std::istringstream iss(upp_paths);
            std::string path;
            while(std::getline(iss, path, ':')) {
                if(!path.empty()) {
                    try {
                        // Check if the path exists and is a directory
                        if(!std::filesystem::exists(path)) {
                            std::cout << "UPP directory does not exist: " << path << "\n";
                            continue;
                        }
                        
                        if(!std::filesystem::is_directory(path)) {
                            std::cout << "UPP path is not a directory: " << path << "\n";
                            continue;
                        }
                        
                        // Create a unique mount point
                        std::string vfs_mount_point = "/mnt/host_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(getpid());
                        
                        // Mount the directory to the VFS
                        vfs.mountFilesystem(path, vfs_mount_point, cwd.primary_overlay);
                        std::cout << "auto-mounted UPP directory: " << path << " -> " << vfs_mount_point << "\n";
                        
                        // Give the mount system time to initialize
                        usleep(300000); // 300ms delay
                        
                        // Recursively scan for .var files and load them
                        std::function<void(const std::string&)> scan_for_var_files = 
                            [&](const std::string& current_vfs_path) {
                                try {
                                    auto overlay_ids = vfs.overlaysForPath(current_vfs_path);
                                    auto listing = vfs.listDir(current_vfs_path, overlay_ids);
                                    
                                    for(const auto& [entry_name, entry] : listing) {
                                        std::string full_entry_path = current_vfs_path + "/" + entry_name;
                                        
                                        // Check if it's a directory
                                        if(entry.types.count('d') > 0) {
                                            // Recursively scan subdirectories
                                            scan_for_var_files(full_entry_path);
                                        }
                                        // Check if it's a .var file
                                        else if(entry_name.length() > 4 && entry_name.substr(entry_name.length() - 4) == ".var") {
                                            std::string var_file_path = full_entry_path;
                                            try {
                                                std::string var_content = vfs.read(var_file_path, std::nullopt);
                                                auto assembly = std::make_shared<UppAssembly>();
                                                if(assembly->load_from_content(var_content, var_file_path)) {
                                                    g_startup_assemblies.push_back(assembly);
                                                    std::cout << "Loaded startup assembly: " << var_file_path << " (from UPP: " << path << ")\n";
                                                    
                                                    // Add to VFS structure
                                                    assembly->create_vfs_structure(vfs, cwd.primary_overlay);
                                                } else {
                                                    std::cout << "Failed to load startup assembly: " << var_file_path << "\n";
                                                }
                                            } catch(const std::exception& e) {
                                                std::cout << "Error loading " << var_file_path << ": " << e.what() << "\n";
                                            }
                                        }
                                    }
                                } catch(const std::exception& e) {
                                    std::cout << "Failed to list directory " << current_vfs_path << ": " << e.what() << "\n";
                                }
                            };
                        
                        // Start scanning from the mount point
                        scan_for_var_files(vfs_mount_point);
                    } catch(const std::exception& e) {
                        std::cout << "Failed to process UPP directory: " << path << " - Exception: " << e.what() << "\n";
                    } catch(...) {
                        std::cout << "Failed to process UPP directory: " << path << " - Unknown exception\n";
                    }
                }
            }
        }
    } catch(const std::exception& e) {
        std::cout << "note: UPP environment variable processing failed: " << e.what() << "\n";
    }

    std::cout << i18n::get(MsgId::WELCOME) << "\n";
    if(interactive) std::cout << i18n::get(MsgId::DISCUSS_HINT) << "\n";
    string line;

    // Daemon mode: run server and exit
    if(daemon_port > 0){
        try {
            run_daemon_server(daemon_port, vfs, env, cwd);
        } catch(const std::exception& e){
            std::cerr << "daemon error: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    size_t repl_iter = 0;
    std::vector<std::string> history;
    load_history(history);
    history.reserve(history.size() + 256);
    bool history_dirty = false;

    // Helper: Classify user intent for discuss command routing
    auto classify_discuss_intent = [](const std::string& user_input) -> DiscussSession::Mode {
        // Convert to lowercase for matching
        std::string lower = user_input;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // Keywords that indicate planning/breakdown needed
        const std::vector<std::string> planning_keywords = {
            "implement", "add feature", "create", "build", "design",
            "refactor", "rewrite", "restructure", "architecture"
        };

        // Keywords that indicate working on existing plans
        const std::vector<std::string> execution_keywords = {
            "do", "execute", "run", "complete", "finish", "continue"
        };

        // Keywords that indicate simple queries
        const std::vector<std::string> simple_keywords = {
            "what is", "how does", "explain", "show me", "tell me",
            "where is", "find", "search"
        };

        // Check for execution keywords first (highest priority for "do")
        for(const auto& kw : execution_keywords){
            if(lower.find(kw) != std::string::npos){
                return DiscussSession::Mode::Execution;
            }
        }

        // Check for planning keywords
        for(const auto& kw : planning_keywords){
            if(lower.find(kw) != std::string::npos){
                return DiscussSession::Mode::Planning;
            }
        }

        // Check for simple query keywords
        for(const auto& kw : simple_keywords){
            if(lower.find(kw) != std::string::npos){
                return DiscussSession::Mode::Simple;
            }
        }

        // Default: if uncertain, return Simple (will ask AI to classify)
        return DiscussSession::Mode::Simple;
    };

    std::function<CommandResult(const CommandInvocation&, const std::string&)> execute_single;
    execute_single = [&](const CommandInvocation& inv, const std::string& stdin_data) -> CommandResult {
        ScopedCoutCapture capture;
        CommandResult result;
        const std::string& cmd = inv.name;

        auto read_path = [&](const std::string& operand) -> std::string {
            std::string abs = normalize_path(cwd.path, operand);
            if(auto node = vfs.tryResolveForOverlay(abs, cwd.primary_overlay)){
                if(node->kind == VfsNode::Kind::Dir)
                    throw std::runtime_error("cannot read directory: " + operand);
                return node->read();
            }
            auto hits = vfs.resolveMulti(abs);
            if(hits.empty()) throw std::runtime_error("path not found: " + operand);
            std::vector<size_t> overlays;
            for(const auto& hit : hits){
                if(hit.node->kind != VfsNode::Kind::Dir)
                    overlays.push_back(hit.overlay_id);
            }
            if(overlays.empty()) throw std::runtime_error("cannot read directory: " + operand);
            sort_unique(overlays);
            size_t chosen = select_overlay(vfs, cwd, overlays);
            auto node = vfs.resolveForOverlay(abs, chosen);
            if(node->kind == VfsNode::Kind::Dir) throw std::runtime_error("cannot read directory: " + operand);
            return node->read();
        };

        if(cmd == "pwd"){
            std::ostringstream oss;
            oss << cwd.path << overlay_suffix(vfs, cwd.overlays, cwd.primary_overlay) << "\n";
            result.output = oss.str();

        } else if(cmd == "cd"){
            std::string target = inv.args.empty() ? std::string("/") : inv.args[0];
            std::string abs = normalize_path(cwd.path, target);
            auto dirOverlays = vfs.overlaysForPath(abs);
            if(dirOverlays.empty()){
                auto hits = vfs.resolveMulti(abs);
                if(hits.empty()) throw std::runtime_error("cd: no such path");
                throw std::runtime_error("cd: not a directory");
            }
            update_directory_context(vfs, cwd, abs);

        } else if(cmd == "ls"){
            std::string abs = inv.args.empty() ? cwd.path : normalize_path(cwd.path, inv.args[0]);
            auto hits = vfs.resolveMulti(abs);
            if(hits.empty()) throw std::runtime_error("ls: path not found");

            bool anyDir = false;
            std::vector<size_t> listingOverlays;
            for(const auto& hit : hits){
                if(hit.node->isDir()){
                    anyDir = true;
                    listingOverlays.push_back(hit.overlay_id);
                } else {
                    listingOverlays.push_back(hit.overlay_id);
                }
            }
            sort_unique(listingOverlays);

            if(anyDir){
                auto listing = vfs.listDir(abs, listingOverlays);
                for(const auto& [name, entry] : listing){
                    std::vector<size_t> ids = entry.overlays;
                    sort_unique(ids);
                    char type = entry.types.size() == 1 ? *entry.types.begin() : '!';
                    std::cout << type << " " << name;
                    if(ids.size() > 1 || (ids.size() == 1 && ids[0] != cwd.primary_overlay)){
                        std::cout << overlay_suffix(vfs, ids, cwd.primary_overlay);
                    }
                    std::cout << "\n";
                }
            } else {
                size_t fileCount = 0;
                std::shared_ptr<VfsNode> node;
                std::vector<size_t> ids;
                for(const auto& hit : hits){
                    if(hit.node->kind != VfsNode::Kind::Dir){
                        ++fileCount;
                        node = hit.node;
                        ids.push_back(hit.overlay_id);
                    }
                }
                if(!node) throw std::runtime_error("ls: unsupported node type");
                sort_unique(ids);
                char type = fileCount > 1 ? '!' : type_char(node);
                std::cout << type << " " << path_basename(abs);
                if(ids.size() > 1 || (ids.size() == 1 && ids[0] != cwd.primary_overlay)){
                    std::cout << overlay_suffix(vfs, ids, cwd.primary_overlay);
                }
                std::cout << "\n";
            }

        } else if(cmd == "tree.adv" || cmd == "tree.advanced"){
            // Advanced tree visualization with options
            std::string abs = inv.args.empty() ? cwd.path : normalize_path(cwd.path, inv.args[0]);
            Vfs::TreeOptions opts;

            // Parse options from remaining args
            for(size_t i = 1; i < inv.args.size(); ++i){
                const std::string& opt = inv.args[i];
                if(opt == "--no-box") opts.use_box_chars = false;
                else if(opt == "--sizes") opts.show_sizes = true;
                else if(opt == "--tags") opts.show_tags = true;
                else if(opt == "--colors") opts.use_colors = true;
                else if(opt == "--kind") opts.show_node_kind = true;
                else if(opt == "--sort") opts.sort_entries = true;
                else if(opt.rfind("--depth=", 0) == 0){
                    opts.max_depth = std::stoi(opt.substr(8));
                } else if(opt.rfind("--filter=", 0) == 0){
                    opts.filter_pattern = opt.substr(9);
                }
            }

            vfs.treeAdvanced(abs, opts);

        } else if(cmd == "tree"){
            std::string abs = inv.args.empty() ? cwd.path : normalize_path(cwd.path, inv.args[0]);
            auto hits = vfs.resolveMulti(abs);
            if(hits.empty()) throw std::runtime_error("tree: path not found");
            std::vector<size_t> ids;
            for(const auto& hit : hits){
                if(hit.node->isDir()) ids.push_back(hit.overlay_id);
            }
            if(ids.empty()) throw std::runtime_error("tree: not a directory");
            sort_unique(ids);

            std::function<void(const std::string&, const std::string&, const std::vector<size_t>&)> dump;
            dump = [&](const std::string& path, const std::string& prefix, const std::vector<size_t>& overlays){
                auto currentHits = vfs.resolveMulti(path, overlays);
                char type = 'd';
                if(!currentHits.empty()){
                    std::set<char> types;
                    for(const auto& h : currentHits) types.insert(type_char(h.node));
                    type = types.size()==1 ? *types.begin() : '!';
                }
                std::cout << prefix << type << " " << path_basename(path) << overlay_suffix(vfs, overlays, cwd.primary_overlay) << "\n";
                auto listing = vfs.listDir(path, overlays);
                for(const auto& [name, entry] : listing){
                    std::string childPath = path == "/" ? join_path(path, name) : join_path(path, name);
                    std::vector<size_t> childIds = entry.overlays;
                    sort_unique(childIds);
                    dump(childPath, prefix + "  ", childIds);
                }
            };

            dump(abs, "", ids);

        } else if(cmd == "mkdir"){
            if(inv.args.empty()) throw std::runtime_error("mkdir <path>");
            std::string abs = normalize_path(cwd.path, inv.args[0]);
            vfs.mkdir(abs, cwd.primary_overlay);

        } else if(cmd == "touch"){
            if(inv.args.empty()) throw std::runtime_error("touch <path>");
            std::string abs = normalize_path(cwd.path, inv.args[0]);
            vfs.touch(abs, cwd.primary_overlay);

        } else if(cmd == "cat"){
            if(inv.args.empty()){
                result.output = stdin_data;
            } else {
                std::ostringstream oss;
                for(size_t i = 0; i < inv.args.size(); ++i){
                    std::string data = read_path(inv.args[i]);
                    oss << data;
                    if(data.empty() || data.back() != '\n') oss << '\n';
                }
                result.output = oss.str();
            }

        } else if(cmd == "grep"){
            if(inv.args.empty()) throw std::runtime_error("grep [-i] <pattern> [path]");
            size_t idx = 0;
            bool ignore_case = false;
            if(inv.args[idx] == "-i"){
                ignore_case = true;
                ++idx;
                if(idx >= inv.args.size()) throw std::runtime_error("grep [-i] <pattern> [path]");
            }
            std::string pattern = inv.args[idx++];
            std::string data = idx < inv.args.size() ? read_path(inv.args[idx]) : stdin_data;
            auto lines = split_lines(data);
            std::ostringstream oss;
            bool matched = false;
            std::string needle = pattern;
            if(ignore_case){
                std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            }
            for(size_t i = 0; i < lines.lines.size(); ++i){
                std::string hay = lines.lines[i];
                if(ignore_case){
                    std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                }
                if(hay.find(needle) != std::string::npos){
                    matched = true;
                    oss << lines.lines[i];
                    bool had_newline = (i < lines.lines.size() - 1) || lines.trailing_newline;
                    if(had_newline) oss << '\n';
                }
            }
            result.output = oss.str();
            result.success = matched;

        } else if(cmd == "rg"){
            if(inv.args.empty()) throw std::runtime_error("rg [-i] <pattern> [path]");
            size_t idx = 0;
            bool ignore_case = false;
            if(inv.args[idx] == "-i"){
                ignore_case = true;
                ++idx;
                if(idx >= inv.args.size()) throw std::runtime_error("rg [-i] <pattern> [path]");
            }
            std::string pattern = inv.args[idx++];
            std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
            if(ignore_case) flags = static_cast<std::regex_constants::syntax_option_type>(flags | std::regex_constants::icase);
            std::regex re;
            try{
                re = std::regex(pattern, flags);
            } catch(const std::regex_error& e){
                throw std::runtime_error(std::string("rg regex error: ") + e.what());
            }
            std::string data = idx < inv.args.size() ? read_path(inv.args[idx]) : stdin_data;
            auto lines = split_lines(data);
            std::ostringstream oss;
            bool matched = false;
            for(size_t i = 0; i < lines.lines.size(); ++i){
                if(std::regex_search(lines.lines[i], re)){
                    matched = true;
                    oss << lines.lines[i];
                    bool had_newline = (i < lines.lines.size() - 1) || lines.trailing_newline;
                    if(had_newline) oss << '\n';
                }
            }
            result.output = oss.str();
            result.success = matched;

        } else if(cmd == "count"){
            std::string data = inv.args.empty() ? stdin_data : read_path(inv.args[0]);
            size_t lines = count_lines(data);
            result.output = std::to_string(lines) + "\n";

        } else if(cmd == "history"){
            bool show_all = false;
            size_t requested = 10;
            size_t idx = 0;
            while(idx < inv.args.size()){
                const std::string& opt = inv.args[idx];
                if(opt == "-a"){
                    show_all = true;
                    ++idx;
                } else if(opt == "-n"){
                    if(idx + 1 >= inv.args.size()) throw std::runtime_error("history -n <count>");
                    requested = parse_size_arg(inv.args[idx + 1], "history count");
                    show_all = false;
                    idx += 2;
                } else {
                    throw std::runtime_error("history [-a | -n <count>]");
                }
            }

            size_t total = history.size();
            size_t start = 0;
            if(!show_all){
                if(requested < total){
                    start = total - requested;
                }
            }
            for(size_t i = start; i < total; ++i){
                std::cout << (i + 1) << "  " << history[i] << "\n";
            }

        } else if(cmd == "true"){
            result.success = true;

        } else if(cmd == "false"){
            result.success = false;

        // Registry commands
        } else if(cmd == "reg.set"){
            if(inv.args.size() < 2){
                throw std::runtime_error("reg.set <key> <value>");
            }
            std::string key_path = inv.args[0];
            std::string value_data = join_args(inv.args, 1);
            g_registry.setValue(key_path, value_data);
            // Sync to VFS
            g_registry.syncToVFS(vfs);

        } else if(cmd == "reg.get"){
            if(inv.args.empty()){
                throw std::runtime_error("reg.get <key>");
            }
            std::string key_path = inv.args[0];
            std::string value = g_registry.getValue(key_path);
            std::cout << value << "\n";

        } else if(cmd == "reg.list"){
            std::string path = inv.args.empty() ? "/" : inv.args[0];
            if(g_registry.exists(path)){
                // List subkeys
                auto subkeys = g_registry.listKeys(path);
                for(const auto& subkey : subkeys){
                    std::cout << subkey << "/\n";
                }
                // List values
                auto values = g_registry.listValues(path);
                for(const auto& value : values){
                    std::cout << value << "\n";
                }
            } else {
                throw std::runtime_error("reg.list: path not found");
            }

        } else if(cmd == "reg.rm"){
            if(inv.args.empty()){
                throw std::runtime_error("reg.rm <key>");
            }
            std::string path = inv.args[0];
            if(g_registry.exists(path)){
                // Check if it's a key or a value
                g_registry.removeKey(path);
                // Sync to VFS
                g_registry.syncToVFS(vfs);
            } else {
                throw std::runtime_error("reg.rm: key not found");
            }

        } else if(cmd == "tail"){
            size_t idx = 0;
            size_t take = 10;
            auto is_number = [](const std::string& s){
                return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isdigit(c); });
            };
            if(idx < inv.args.size()){
                if(inv.args[idx] == "-n"){
                    if(idx + 1 >= inv.args.size()) throw std::runtime_error("tail -n <count> [path]");
                    take = parse_size_arg(inv.args[idx + 1], "tail count");
                    idx += 2;
                } else if(inv.args.size() - idx > 1 && is_number(inv.args[idx])){
                    take = parse_size_arg(inv.args[idx], "tail count");
                    ++idx;
                }
            }
            std::string data = idx < inv.args.size() ? read_path(inv.args[idx]) : stdin_data;
            auto lines = split_lines(data);
            size_t total = lines.lines.size();
            size_t begin = take >= total ? 0 : total - take;
            result.output = join_line_range(lines, begin, total);

        } else if(cmd == "head"){
            size_t idx = 0;
            size_t take = 10;
            auto is_number = [](const std::string& s){
                return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isdigit(c); });
            };
            if(idx < inv.args.size()){
                if(inv.args[idx] == "-n"){
                    if(idx + 1 >= inv.args.size()) throw std::runtime_error("head -n <count> [path]");
                    take = parse_size_arg(inv.args[idx + 1], "head count");
                    idx += 2;
                } else if(inv.args.size() - idx > 1 && is_number(inv.args[idx])){
                    take = parse_size_arg(inv.args[idx], "head count");
                    ++idx;
                }
            }
            std::string data = idx < inv.args.size() ? read_path(inv.args[idx]) : stdin_data;
            auto lines = split_lines(data);
            size_t end = std::min(take, lines.lines.size());
            result.output = join_line_range(lines, 0, end);

        } else if(cmd == "uniq"){
            std::string data = inv.args.empty() ? stdin_data : read_path(inv.args[0]);
            auto lines = split_lines(data);
            std::ostringstream oss;
            std::string prev;
            bool have_prev = false;
            for(size_t i = 0; i < lines.lines.size(); ++i){
                const auto& line = lines.lines[i];
                if(!have_prev || line != prev){
                    oss << line;
                    bool had_newline = (i < lines.lines.size() - 1) || lines.trailing_newline;
                    if(had_newline) oss << '\n';
                    prev = line;
                    have_prev = true;
                }
            }
            result.output = oss.str();

        } else if(cmd == "random"){
            long long lo = 0;
            long long hi = 1000000;
            if(inv.args.size() > 2) throw std::runtime_error("random [min [max]]");
            if(inv.args.size() == 1){
                hi = parse_int_arg(inv.args[0], "random max");
            } else if(inv.args.size() >= 2){
                lo = parse_int_arg(inv.args[0], "random min");
                hi = parse_int_arg(inv.args[1], "random max");
            }
            if(lo > hi) throw std::runtime_error("random range invalid (min > max)");
            std::uniform_int_distribution<long long> dist(lo, hi);
            long long value = dist(rng());
            result.output = std::to_string(value) + "\n";

        } else if(cmd == "echo"){
            // Bourne shell compatible echo - prints to stdout
            std::string text = join_args(inv.args, 0);
            result.output = text + "\n";

        } else if(cmd == "rm"){
            if(inv.args.empty()) throw std::runtime_error("rm <path>");
            std::string abs = normalize_path(cwd.path, inv.args[0]);
            vfs.rm(abs, cwd.primary_overlay);

        } else if(cmd == "mv"){
            if(inv.args.size() < 2) throw std::runtime_error("mv <src> <dst>");
            std::string absSrc = normalize_path(cwd.path, inv.args[0]);
            std::string absDst = normalize_path(cwd.path, inv.args[1]);
            vfs.mv(absSrc, absDst, cwd.primary_overlay);

        } else if(cmd == "link"){
            if(inv.args.size() < 2) throw std::runtime_error("link <src> <dst>");
            std::string absSrc = normalize_path(cwd.path, inv.args[0]);
            std::string absDst = normalize_path(cwd.path, inv.args[1]);
            vfs.link(absSrc, absDst, cwd.primary_overlay);

        } else if(cmd == "export"){
            if(inv.args.size() < 2) throw std::runtime_error("export <vfs> <host>");
            std::string data = read_path(inv.args[0]);
            std::ofstream out(inv.args[1], std::ios::binary);
            if(!out) throw std::runtime_error("export: cannot open host file");
            out.write(data.data(), static_cast<std::streamsize>(data.size()));
            std::cout << "export -> " << inv.args[1] << "\n";

        } else if(cmd == "parse"){
            if(inv.args.size() < 2) throw std::runtime_error("parse <src> <dst>");
            std::string absDst = normalize_path(cwd.path, inv.args[1]);
            auto text = read_path(inv.args[0]);
            auto ast = parse(text);
            auto holder = std::make_shared<AstHolder>(path_basename(absDst), ast);
            std::string dir = absDst.substr(0, absDst.find_last_of('/'));
            if(dir.empty()) dir = "/";
            vfs.addNode(dir, holder, cwd.primary_overlay);
            std::cout << "AST @ " << absDst << " valmis.\n";

        } else if(cmd == "eval"){
            if(inv.args.empty()) throw std::runtime_error("eval <path>");
            std::string abs = normalize_path(cwd.path, inv.args[0]);
            std::shared_ptr<VfsNode> n;
            try{
                n = vfs.resolveForOverlay(abs, cwd.primary_overlay);
            } catch(const std::exception&){
                auto hits = vfs.resolveMulti(abs);
                if(hits.empty()) throw;
                std::vector<size_t> overlays;
                for(const auto& h : hits){
                    if(h.node->kind == VfsNode::Kind::Ast || h.node->kind == VfsNode::Kind::File)
                        overlays.push_back(h.overlay_id);
                }
                sort_unique(overlays);
                size_t chosen = select_overlay(vfs, cwd, overlays);
                n = vfs.resolveForOverlay(abs, chosen);
            }
            if(n->kind != VfsNode::Kind::Ast) throw std::runtime_error("not AST");
            auto a = std::dynamic_pointer_cast<AstNode>(n);
            auto val = a->eval(env);
            std::cout << val.show() << "\n";

        } else if(cmd == "ai"){
            // Alias for discuss (interactive AI discussion)
            // Redirect to discuss handler
            CommandInvocation discuss_inv;
            discuss_inv.name = "discuss";
            discuss_inv.args = inv.args;
            return execute_single(discuss_inv, stdin_data);

        } else if(cmd == "ai.raw"){
            // Raw AI call without discussion context
            std::string prompt = join_args(inv.args);
            if(prompt.empty()){
                std::cout << "anna promptti.\n";
                result.success = false;
            } else {
                result.output = call_ai(prompt);
            }

        } else if(cmd == "ai.brief"){
            if(inv.args.empty()) throw std::runtime_error("ai.brief <key> [extra...]");
            auto key = inv.args[0];
            std::optional<std::string> prompt;
            if(key == "ai-bridge-hello" || key == "bridge.hello" || key == "bridge-hello"){
                prompt = snippets::ai_bridge_hello_briefing();
            }

            if(!prompt || prompt->empty()){
                std::cout << "unknown briefing key\n";
                result.success = false;
            } else {
                if(inv.args.size() > 1){
                    auto extra = join_args(inv.args, 1);
                    if(!extra.empty()){
                        if(!prompt->empty() && prompt->back() != '\n') prompt->push_back(' ');
                        prompt->append(extra);
                    }
                }
                result.output = call_ai(*prompt);
            }

        } else if(cmd == "discuss" || cmd == "ai.discuss"){
            std::string user_input = join_args(inv.args);
            if(user_input.empty()){
                // Enter interactive discuss sub-loop
                if(!discuss.is_active()){
                    discuss.session_id = discuss.generate_session_id();
                    std::cout << "📝 Started discussion session: " << discuss.session_id << "\n";
                }
                std::cout << "💬 Entering interactive discussion mode. Type 'exit' or 'back' to return.\n";
                std::string sub_line;
                std::vector<std::string> sub_history;
                while(true){
                    if(!read_line_with_history(vfs, "discuss> ", sub_line, sub_history, cwd.path)){
                        break;
                    }
                    auto trimmed = trim_copy(sub_line);
                    if(trimmed.empty()) continue;
                    if(trimmed == "exit" || trimmed == "back" || trimmed == "quit"){
                        std::cout << "👋 Exiting discuss mode\n";
                        break;
                    }

                    // Process as discuss command with argument
                    CommandInvocation sub_inv;
                    sub_inv.name = "discuss";
                    sub_inv.args.push_back(trimmed);
                    try{
                        auto sub_result = execute_single(sub_inv, "");
                        if(!sub_result.output.empty()){
                            std::cout << sub_result.output;
                            std::cout.flush();
                        }
                    } catch(const std::exception& e){
                        std::cout << "error: " << e.what() << "\n";
                    }
                }
            } else {
                // Start or continue session
                if(!discuss.is_active()){
                    discuss.session_id = discuss.generate_session_id();
                    std::cout << "📝 Started discussion session: " << discuss.session_id << "\n";
                }

                // Classify intent
                auto intent = classify_discuss_intent(user_input);
                discuss.mode = intent;
                discuss.add_message("user", user_input);

                // Route based on mode
                if(intent == DiscussSession::Mode::Simple){
                    // Simple query - just call AI directly
                    std::cout << "🤔 Thinking...\n";
                    result.output = call_ai(user_input);
                    discuss.add_message("assistant", result.output);

                } else if(intent == DiscussSession::Mode::Execution){
                    // Check if plan exists
                    auto plan_exists = vfs.tryResolveForOverlay("/plan", 0) &&
                                      vfs.tryResolveForOverlay("/plan", 0)->isDir();

                    if(!plan_exists){
                        // No plan - redirect to plan.discuss
                        std::cout << "⚠️  No plan found in /plan tree. Let's create one first.\n";
                        std::cout << "→ Switching to planning mode...\n";
                        discuss.mode = DiscussSession::Mode::Planning;
                        // Fall through to planning mode
                        intent = DiscussSession::Mode::Planning;
                    } else {
                        // Execute pre-planned work
                        std::cout << "⚙️  Executing planned work...\n";
                        std::string execution_prompt =
                            "The user wants to execute this task: " + user_input + "\n" +
                            "Review the plan in /plan tree and execute the appropriate steps.\n" +
                            "Available commands: " + snippets::tool_list();
                        result.output = call_ai(execution_prompt);
                        discuss.add_message("assistant", result.output);
                    }
                }

                if(intent == DiscussSession::Mode::Planning){
                    // Planning mode - create or update plans
                    std::cout << "📋 Planning mode activated\n";
                    std::cout << "🔍 Analyzing request and breaking down into steps...\n";

                    std::string planning_prompt =
                        "User request: " + user_input + "\n\n" +
                        "Break this down into a structured plan. Create or update plan nodes in /plan tree.\n" +
                        "Use commands like: plan.create, plan.goto, plan.jobs.add\n" +
                        "Ask clarifying questions if needed (format: Q: <question>)\n" +
                        "Available commands: " + snippets::tool_list();

                    result.output = call_ai(planning_prompt);
                    discuss.add_message("assistant", result.output);

                    // TODO: Parse AI response for questions, plan updates, etc.
                    // For now, just display the response
                }
            }

        } else if(cmd == "discuss.session"){
            if(inv.args.empty()){
                std::cout << "discuss.session new [name] | end\n";
                result.success = false;
            } else {
                auto subcmd = inv.args[0];
                if(subcmd == "new"){
                    if(discuss.is_active()){
                        std::cout << "⚠️  Ending previous session: " << discuss.session_id << "\n";
                    }
                    discuss.clear();
                    if(inv.args.size() > 1){
                        discuss.session_id = inv.args[1];
                    } else {
                        discuss.session_id = discuss.generate_session_id();
                    }
                    std::cout << "✨ New session: " << discuss.session_id << "\n";
                } else if(subcmd == "end"){
                    if(discuss.is_active()){
                        std::cout << "✅ Ended session: " << discuss.session_id << "\n";
                        discuss.clear();
                    } else {
                        std::cout << "⚠️  No active session\n";
                    }
                } else {
                    std::cout << "unknown subcommand: " << subcmd << "\n";
                    result.success = false;
                }
            }

        } else if(cmd == "tools"){
            auto tools = snippets::tool_list();
            std::cout << tools;
            if(tools.empty() || tools.back() != '\n') std::cout << '\n';

        } else if(cmd == "overlay.list"){
            for(size_t i = 0; i < vfs.overlayCount(); ++i){
                bool in_scope = std::find(cwd.overlays.begin(), cwd.overlays.end(), i) != cwd.overlays.end();
                bool primary = (i == cwd.primary_overlay);
                std::cout << (primary ? '*' : ' ') << (in_scope ? '+' : ' ') << " [" << i << "] " << vfs.overlayName(i) << "\n";
            }
            std::cout << "policy: " << policy_label(cwd.conflict_policy) << "\n";

        } else if(cmd == "overlay.use"){
            if(inv.args.empty()) throw std::runtime_error("overlay.use <name>");
            auto name = inv.args[0];
            auto idOpt = vfs.findOverlayByName(name);
            if(!idOpt) throw std::runtime_error("overlay: unknown overlay");
            size_t id = *idOpt;
            if(std::find(cwd.overlays.begin(), cwd.overlays.end(), id) == cwd.overlays.end())
                throw std::runtime_error("overlay not active in current directory");
            cwd.primary_overlay = id;

        } else if(cmd == "overlay.policy"){
            if(inv.args.empty()){
                std::cout << "overlay policy: " << policy_label(cwd.conflict_policy) << " (manual|oldest|newest)\n";
            } else {
                auto parsed = parse_policy(inv.args[0]);
                if(!parsed) throw std::runtime_error("overlay.policy manual|oldest|newest");
                cwd.conflict_policy = *parsed;
                update_directory_context(vfs, cwd, cwd.path);
                std::cout << "overlay policy set to " << policy_label(cwd.conflict_policy) << "\n";
            }

        } else if(cmd == "overlay.mount"){
            if(inv.args.size() < 2) throw std::runtime_error("overlay.mount <name> <file>");
            size_t id = mount_overlay_from_file(vfs, inv.args[0], inv.args[1]);
            maybe_extend_context(vfs, cwd);
            std::cout << "mounted overlay " << inv.args[0] << " (#" << id << ")\n";

        } else if(cmd == "overlay.save"){
            if(inv.args.size() < 2) throw std::runtime_error("overlay.save <name> <file>");
            auto idOpt = vfs.findOverlayByName(inv.args[0]);
            if(!idOpt) throw std::runtime_error("overlay: unknown overlay");
            save_overlay_to_file(vfs, *idOpt, inv.args[1]);
            if(solution.active && *idOpt == solution.overlay_id){
                std::filesystem::path p = inv.args[1];
                try{
                    if(p.is_relative()) p = std::filesystem::absolute(p);
                } catch(const std::exception&){ }
                solution.file_path = p.string();
            }
            std::cout << "overlay " << inv.args[0] << " (#" << *idOpt << ") -> " << inv.args[1] << "\n";

        } else if(cmd == "overlay.unmount"){
            if(inv.args.empty()) throw std::runtime_error("overlay.unmount <name>");
            auto idOpt = vfs.findOverlayByName(inv.args[0]);
            if(!idOpt) throw std::runtime_error("overlay: unknown overlay");
            if(*idOpt == 0) throw std::runtime_error("cannot unmount base overlay");
            vfs.unregisterOverlay(*idOpt);
            adjust_context_after_unmount(vfs, cwd, *idOpt);

        } else if(cmd == "mount"){
            if(inv.args.size() < 2) throw std::runtime_error("mount <host-path> <vfs-path>");
            std::string host_path = inv.args[0];
            std::string vfs_path = normalize_path(cwd.path, inv.args[1]);
            vfs.mountFilesystem(host_path, vfs_path, cwd.primary_overlay);
            std::cout << "mounted " << host_path << " -> " << vfs_path << "\n";

        } else if(cmd == "mount.lib"){
            if(inv.args.size() < 2) throw std::runtime_error("mount.lib <lib-path> <vfs-path>");
            std::string lib_path = inv.args[0];
            std::string vfs_path = normalize_path(cwd.path, inv.args[1]);
            vfs.mountLibrary(lib_path, vfs_path, cwd.primary_overlay);
            std::cout << "mounted library " << lib_path << " -> " << vfs_path << "\n";

        } else if(cmd == "mount.remote"){
            if(inv.args.size() < 4) throw std::runtime_error("mount.remote <host> <port> <remote-vfs-path> <local-vfs-path>");
            std::string host = inv.args[0];
            int port = std::stoi(inv.args[1]);
            std::string remote_path = inv.args[2];
            std::string vfs_path = normalize_path(cwd.path, inv.args[3]);
            vfs.mountRemote(host, port, remote_path, vfs_path, cwd.primary_overlay);
            std::cout << "mounted remote " << host << ":" << port << ":" << remote_path << " -> " << vfs_path << "\n";

        } else if(cmd == "mount.list"){
            auto mounts = vfs.listMounts();
            if(mounts.empty()){
                std::cout << "no mounts\n";
            } else {
                for(const auto& m : mounts){
                    std::string type_marker;
                    switch(m.type){
                        case Vfs::MountType::Filesystem: type_marker = "m "; break;
                        case Vfs::MountType::Library: type_marker = "l "; break;
                        case Vfs::MountType::Remote: type_marker = "r "; break;
                    }
                    std::cout << type_marker << m.vfs_path << " <- " << m.host_path << "\n";
                }
            }
            std::cout << "mounting " << (vfs.isMountAllowed() ? "allowed" : "disabled") << "\n";

        } else if(cmd == "mount.allow"){
            vfs.setMountAllowed(true);
            std::cout << "mounting enabled\n";

        } else if(cmd == "mount.disallow"){
            vfs.setMountAllowed(false);
            std::cout << "mounting disabled (existing mounts remain active)\n";

        } else if(cmd == "unmount"){
            if(inv.args.empty()) throw std::runtime_error("unmount <vfs-path>");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            vfs.unmount(vfs_path);
            std::cout << "unmounted " << vfs_path << "\n";

        } else if(cmd == "tag.add"){
            if(inv.args.size() < 2) throw std::runtime_error("tag.add <vfs-path> <tag-name> [tag-name...]");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            for(size_t i = 1; i < inv.args.size(); ++i){
                vfs.addTag(vfs_path, inv.args[i]);
            }
            std::cout << "tagged " << vfs_path << " with " << (inv.args.size() - 1) << " tag(s)\n";

        } else if(cmd == "tag.remove"){
            if(inv.args.size() < 2) throw std::runtime_error("tag.remove <vfs-path> <tag-name> [tag-name...]");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            for(size_t i = 1; i < inv.args.size(); ++i){
                vfs.removeTag(vfs_path, inv.args[i]);
            }
            std::cout << "removed " << (inv.args.size() - 1) << " tag(s) from " << vfs_path << "\n";

        } else if(cmd == "tag.list"){
            if(inv.args.empty()){
                // List all registered tags
                auto tags = vfs.allRegisteredTags();
                if(tags.empty()){
                    std::cout << "no tags registered\n";
                } else {
                    std::cout << "registered tags (" << tags.size() << "):\n";
                    for(const auto& tag : tags){
                        std::cout << "  " << tag << "\n";
                    }
                }
            } else {
                // List tags for specific path
                std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
                auto tags = vfs.getNodeTags(vfs_path);
                if(tags.empty()){
                    std::cout << vfs_path << ": no tags\n";
                } else {
                    std::cout << vfs_path << ": ";
                    for(size_t i = 0; i < tags.size(); ++i){
                        if(i > 0) std::cout << ", ";
                        std::cout << tags[i];
                    }
                    std::cout << "\n";
                }
            }

        } else if(cmd == "tag.clear"){
            if(inv.args.empty()) throw std::runtime_error("tag.clear <vfs-path>");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            vfs.clearNodeTags(vfs_path);
            std::cout << "cleared all tags from " << vfs_path << "\n";

        } else if(cmd == "tag.has"){
            if(inv.args.size() < 2) throw std::runtime_error("tag.has <vfs-path> <tag-name>");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            bool has = vfs.nodeHasTag(vfs_path, inv.args[1]);
            std::cout << vfs_path << (has ? " has " : " does not have ") << "tag '" << inv.args[1] << "'\n";

        } else if(cmd == "logic.init"){
            vfs.logic_engine.addHardcodedRules();
            std::cout << "initialized logic engine with " << vfs.logic_engine.rules.size() << " hardcoded rules\n";

        } else if(cmd == "logic.infer"){
            if(inv.args.empty()) throw std::runtime_error("logic.infer <tag> [tag...]");

            TagSet initial_tags;
            for(const auto& tag_name : inv.args){
                TagId tid = vfs.registerTag(tag_name);
                initial_tags.insert(tid);
            }

            TagSet inferred = vfs.logic_engine.inferTags(initial_tags);

            std::cout << "initial tags: ";
            for(TagId tid : initial_tags){
                std::cout << vfs.getTagName(tid) << " ";
            }
            std::cout << "\ninferred tags (only new): ";
            for(TagId tid : inferred){
                if(initial_tags.count(tid) == 0){
                    std::cout << vfs.getTagName(tid) << " ";
                }
            }
            std::cout << "\ncomplete tag set (initial + inferred): ";
            for(TagId tid : inferred){
                std::cout << vfs.getTagName(tid) << " ";
            }
            std::cout << "\n";

        } else if(cmd == "logic.check"){
            if(inv.args.empty()) throw std::runtime_error("logic.check <tag> [tag...]");

            TagSet tags;
            for(const auto& tag_name : inv.args){
                TagId tid = vfs.registerTag(tag_name);
                tags.insert(tid);
            }

            auto conflict = vfs.logic_engine.checkConsistency(tags);
            if(conflict){
                std::cout << "CONFLICT: " << conflict->description << "\n";
                std::cout << "conflicting tags: ";
                for(const auto& tag : conflict->conflicting_tags){
                    std::cout << tag << " ";
                }
                std::cout << "\nsuggestions:\n";
                for(const auto& suggestion : conflict->suggestions){
                    std::cout << "  - " << suggestion << "\n";
                }
            } else {
                std::cout << "tags are consistent\n";
            }

        } else if(cmd == "logic.explain"){
            if(inv.args.size() < 2) throw std::runtime_error("logic.explain <target-tag> <source-tag> [source-tag...]");

            std::string target_tag_name = inv.args[0];
            TagId target_tag = vfs.registerTag(target_tag_name);

            TagSet source_tags;
            for(size_t i = 1; i < inv.args.size(); ++i){
                TagId tid = vfs.registerTag(inv.args[i]);
                source_tags.insert(tid);
            }

            auto explanations = vfs.logic_engine.explainInference(target_tag, source_tags);
            for(const auto& exp : explanations){
                std::cout << exp << "\n";
            }

        } else if(cmd == "logic.listrules"){
            if(vfs.logic_engine.rules.empty()){
                std::cout << "no rules loaded (use logic.init to add hardcoded rules)\n";
            } else {
                std::cout << "loaded rules (" << vfs.logic_engine.rules.size() << "):\n";
                for(const auto& rule : vfs.logic_engine.rules){
                    std::cout << "  " << rule.name << ": "
                             << rule.premise->toString(vfs.tag_registry) << " => "
                             << rule.conclusion->toString(vfs.tag_registry)
                             << " [" << int(rule.confidence * 100) << "%, " << rule.source << "]\n";
                }
            }

        } else if(cmd == "logic.rules.save"){
            std::string path = inv.args.empty() ? "/plan/rules" : inv.args[0];
            vfs.logic_engine.saveRulesToVfs(vfs, path);
            std::cout << "saved " << vfs.logic_engine.rules.size() << " rules to " << path << "\n";

        } else if(cmd == "logic.rules.load"){
            std::string path = inv.args.empty() ? "/plan/rules" : inv.args[0];
            size_t before = vfs.logic_engine.rules.size();
            vfs.logic_engine.loadRulesFromVfs(vfs, path);
            size_t after = vfs.logic_engine.rules.size();
            std::cout << "loaded " << after << " rules from " << path;
            if (before > 0) {
                std::cout << " (replaced " << before << " existing rules)";
            }
            std::cout << "\n";

        } else if(cmd == "logic.rule.add"){
            // logic.rule.add <name> <premise-tag> <conclusion-tag> [confidence] [source]
            if(inv.args.size() < 3) throw std::runtime_error("logic.rule.add <name> <premise-tag> <conclusion-tag> [confidence] [source]");
            std::string name = inv.args[0];
            std::string premise = inv.args[1];
            std::string conclusion = inv.args[2];
            float confidence = inv.args.size() > 3 ? std::stof(inv.args[3]) : 1.0f;
            std::string source = inv.args.size() > 4 ? inv.args[4] : "user";

            vfs.logic_engine.addSimpleRule(name, premise, conclusion, confidence, source);
            std::cout << "added rule: " << name << " (" << premise << " => " << conclusion
                      << ", confidence=" << int(confidence * 100) << "%, source=" << source << ")\n";

        } else if(cmd == "logic.rule.exclude"){
            // logic.rule.exclude <name> <tag1> <tag2> [source]
            if(inv.args.size() < 3) throw std::runtime_error("logic.rule.exclude <name> <tag1> <tag2> [source]");
            std::string name = inv.args[0];
            std::string tag1 = inv.args[1];
            std::string tag2 = inv.args[2];
            std::string source = inv.args.size() > 3 ? inv.args[3] : "user";

            vfs.logic_engine.addExclusionRule(name, tag1, tag2, source);
            std::cout << "added exclusion rule: " << name << " (" << tag1 << " excludes " << tag2
                      << ", source=" << source << ")\n";

        } else if(cmd == "logic.rule.remove"){
            // logic.rule.remove <name>
            if(inv.args.empty()) throw std::runtime_error("logic.rule.remove <name>");
            std::string name = inv.args[0];
            if(vfs.logic_engine.hasRule(name)){
                vfs.logic_engine.removeRule(name);
                std::cout << "removed rule: " << name << "\n";
            } else {
                std::cout << "rule not found: " << name << "\n";
            }

        } else if(cmd == "logic.sat"){
            if(inv.args.empty()) throw std::runtime_error("logic.sat <tag> [tag...]");

            // Build a conjunction of all tags
            std::vector<std::shared_ptr<LogicFormula>> vars;
            for(const auto& tag_name : inv.args){
                TagId tid = vfs.registerTag(tag_name);
                vars.push_back(LogicFormula::makeVar(tid));
            }
            auto formula = LogicFormula::makeAnd(vars);

            bool sat = vfs.logic_engine.isSatisfiable(formula);
            std::cout << "formula is " << (sat ? "satisfiable" : "unsatisfiable") << "\n";

        } else if(cmd == "tag.mine.start"){
            if(inv.args.empty()) throw std::runtime_error("tag.mine.start <tag> [tag...]");

            vfs.mining_session = TagMiningSession();
            for(const auto& tag_name : inv.args){
                TagId tid = vfs.registerTag(tag_name);
                vfs.mining_session->addUserTag(tid);
            }

            // Infer additional tags
            vfs.mining_session->inferred_tags = vfs.logic_engine.inferTags(vfs.mining_session->user_provided_tags);

            // Generate questions for user
            for(TagId tid : vfs.mining_session->inferred_tags){
                if(vfs.mining_session->user_provided_tags.count(tid) == 0){
                    vfs.mining_session->pending_questions.push_back(
                        "Do you also want tag '" + vfs.getTagName(tid) + "'?");
                }
            }

            std::cout << "started tag mining session\n";
            std::cout << "user provided: ";
            for(TagId tid : vfs.mining_session->user_provided_tags){
                std::cout << vfs.getTagName(tid) << " ";
            }
            std::cout << "\ninferred tags: ";
            for(TagId tid : vfs.mining_session->inferred_tags){
                if(vfs.mining_session->user_provided_tags.count(tid) == 0){
                    std::cout << vfs.getTagName(tid) << " ";
                }
            }
            std::cout << "\npending questions: " << vfs.mining_session->pending_questions.size() << "\n";
            if(!vfs.mining_session->pending_questions.empty()){
                std::cout << "\nnext question: " << vfs.mining_session->pending_questions[0] << "\n";
                std::cout << "use: tag.mine.feedback <tag-name> yes|no\n";
            }

        } else if(cmd == "tag.mine.feedback"){
            if(!vfs.mining_session){
                throw std::runtime_error("no active mining session (use tag.mine.start first)");
            }
            if(inv.args.size() < 2) throw std::runtime_error("tag.mine.feedback <tag-name> yes|no");

            std::string tag_name = inv.args[0];
            bool confirmed = (inv.args[1] == "yes" || inv.args[1] == "y");

            vfs.mining_session->recordFeedback(tag_name, confirmed);

            if(confirmed){
                TagId tid = vfs.registerTag(tag_name);
                vfs.mining_session->user_provided_tags.insert(tid);
                std::cout << "added '" << tag_name << "' to user tags\n";
            } else {
                std::cout << "rejected '" << tag_name << "'\n";
            }

        } else if(cmd == "tag.mine.status"){
            if(!vfs.mining_session){
                std::cout << "no active mining session\n";
            } else {
                std::cout << "mining session active\n";
                std::cout << "user tags: ";
                for(TagId tid : vfs.mining_session->user_provided_tags){
                    std::cout << vfs.getTagName(tid) << " ";
                }
                std::cout << "\ninferred tags: ";
                for(TagId tid : vfs.mining_session->inferred_tags){
                    if(vfs.mining_session->user_provided_tags.count(tid) == 0){
                        std::cout << vfs.getTagName(tid) << " ";
                    }
                }
                std::cout << "\nfeedback recorded: " << vfs.mining_session->user_feedback.size() << "\n";
            }

        } else if(cmd == "plan.create"){
            if(inv.args.size() < 2) throw std::runtime_error("plan.create <path> <type> [content]");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            std::string type = inv.args[1];
            std::string content;
            if(inv.args.size() > 2){
                for(size_t i = 2; i < inv.args.size(); ++i){
                    if(i > 2) content += " ";
                    content += inv.args[i];
                }
            }

            std::shared_ptr<PlanNode> node;
            if(type == "root"){
                node = std::make_shared<PlanRoot>(path_basename(vfs_path), content);
            } else if(type == "subplan"){
                node = std::make_shared<PlanSubPlan>(path_basename(vfs_path), content);
            } else if(type == "goals"){
                node = std::make_shared<PlanGoals>(path_basename(vfs_path));
            } else if(type == "ideas"){
                node = std::make_shared<PlanIdeas>(path_basename(vfs_path));
            } else if(type == "strategy"){
                node = std::make_shared<PlanStrategy>(path_basename(vfs_path), content);
            } else if(type == "jobs"){
                node = std::make_shared<PlanJobs>(path_basename(vfs_path));
            } else if(type == "deps"){
                node = std::make_shared<PlanDeps>(path_basename(vfs_path));
            } else if(type == "implemented"){
                node = std::make_shared<PlanImplemented>(path_basename(vfs_path));
            } else if(type == "research"){
                node = std::make_shared<PlanResearch>(path_basename(vfs_path));
            } else if(type == "notes"){
                node = std::make_shared<PlanNotes>(path_basename(vfs_path), content);
            } else {
                throw std::runtime_error("plan.create: unknown type '" + type + "' (valid: root, subplan, goals, ideas, strategy, jobs, deps, implemented, research, notes)");
            }

            vfs_add(vfs, vfs_path, node, cwd.primary_overlay);
            std::cout << "created plan node (" << type << ") @ " << vfs_path << "\n";

            // Check if the parent path has any tags - inherit and verify consistency
            auto parent_path = vfs_path.substr(0, vfs_path.find_last_of('/'));
            if(parent_path.empty()) parent_path = "/";
            auto parent_node = vfs.tryResolveForOverlay(parent_path, cwd.primary_overlay);
            if(parent_node){
                auto parent_tags_ptr = vfs.tag_storage.getTags(parent_node.get());
                if(parent_tags_ptr && !parent_tags_ptr->empty()){
                    // Check for conflicts in parent's tag set (inferred)
                    auto complete_parent_tags = vfs.logic_engine.inferTags(*parent_tags_ptr, 0.8f);
                    auto conflict = vfs.logic_engine.checkConsistency(complete_parent_tags);
                    if(conflict){
                        std::cout << "⚠️  Warning: Parent plan node has conflicting tags\n";
                        std::cout << "   " << conflict->description << "\n";
                        std::cout << "   Use 'plan.verify " << parent_path << "' to see details\n";
                    }
                }
            }

        } else if(cmd == "plan.goto"){
            if(inv.args.empty()) throw std::runtime_error("plan.goto <path>");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            auto node = vfs.tryResolveForOverlay(vfs_path, cwd.primary_overlay);
            if(!node) throw std::runtime_error("plan.goto: path not found: " + vfs_path);
            planner.navigateTo(vfs_path);
            std::cout << "planner now at: " << planner.current_path << "\n";

        } else if(cmd == "plan.forward"){
            planner.forward();
            std::cout << "📍 Planner mode: Forward (adding details to plans)\n";

            // Suggest next steps based on current location
            if(!planner.current_path.empty()){
                auto node = vfs.tryResolveForOverlay(planner.current_path, cwd.primary_overlay);
                if(node && node->isDir()){
                    auto& ch = node->children();
                    if(ch.empty()){
                        std::cout << "💡 Current node has no children. Suggestions:\n";
                        std::cout << "   - Use 'plan.discuss' to break down this plan into steps\n";
                        std::cout << "   - Use 'plan.create <path> goals' to add goal nodes\n";
                        std::cout << "   - Use 'plan.create <path> jobs' to add job tracking\n";
                    } else {
                        std::cout << "📂 Current node has " << ch.size() << " child(ren). Suggestions:\n";
                        std::cout << "   - Use 'plan.goto <child-path>' to drill into details\n";
                        std::cout << "   - Use 'plan.discuss' to add more details\n";
                        std::cout << "   Children: ";
                        bool first = true;
                        for(const auto& [child_name, child_node] : ch){
                            if(!first) std::cout << ", ";
                            std::cout << child_name;
                            first = false;
                        }
                        std::cout << "\n";
                    }
                }
            } else {
                std::cout << "💡 No current location. Use 'plan.goto /plan' to start\n";
            }

        } else if(cmd == "plan.backward"){
            planner.backward();
            std::cout << "📍 Planner mode: Backward (revising high-level plans)\n";

            // Show navigation history and parent path
            if(!planner.current_path.empty()){
                std::cout << "📍 Current: " << planner.current_path << "\n";

                // Find parent path
                auto last_slash = planner.current_path.find_last_of('/');
                if(last_slash != std::string::npos && last_slash > 0){
                    std::string parent_path = planner.current_path.substr(0, last_slash);
                    std::cout << "⬆️  Parent: " << parent_path << "\n";
                    std::cout << "💡 Suggestions:\n";
                    std::cout << "   - Use 'plan.discuss' to revise the current strategy\n";
                    std::cout << "   - Use 'plan.goto " << parent_path << "' to move to parent\n";
                    std::cout << "   - Use 'hypothesis.*' commands to test alternative approaches\n";
                } else {
                    std::cout << "ℹ️  At root level\n";
                    std::cout << "💡 Use 'plan.discuss' to revise overall strategy\n";
                }
            } else {
                std::cout << "⚠️  No current location. Use 'plan.goto <path>' first\n";
            }

            if(!planner.navigation_history.empty()){
                std::cout << "📜 History (" << planner.navigation_history.size() << " entries)\n";
            }

        } else if(cmd == "plan.context.add"){
            if(inv.args.empty()) throw std::runtime_error("plan.context.add <vfs-path> [vfs-path...]");
            for(const auto& arg : inv.args){
                std::string vfs_path = normalize_path(cwd.path, arg);
                planner.addToContext(vfs_path);
            }
            std::cout << "added " << inv.args.size() << " path(s) to planner context\n";

        } else if(cmd == "plan.context.remove"){
            if(inv.args.empty()) throw std::runtime_error("plan.context.remove <vfs-path> [vfs-path...]");
            for(const auto& arg : inv.args){
                std::string vfs_path = normalize_path(cwd.path, arg);
                planner.removeFromContext(vfs_path);
            }
            std::cout << "removed " << inv.args.size() << " path(s) from planner context\n";

        } else if(cmd == "plan.context.clear"){
            planner.clearContext();
            std::cout << "cleared planner context\n";

        } else if(cmd == "plan.context.list"){
            if(planner.visible_nodes.empty()){
                std::cout << "planner context is empty\n";
            } else {
                std::cout << "planner context (" << planner.visible_nodes.size() << " paths):\n";
                for(const auto& path : planner.visible_nodes){
                    std::cout << "  " << path << "\n";
                }
            }

        } else if(cmd == "plan.status"){
            std::cout << "planner status:\n";
            std::cout << "  current: " << planner.current_path << "\n";
            std::cout << "  mode: " << (planner.mode == PlannerContext::Mode::Forward ? "forward" : "backward") << "\n";
            std::cout << "  context size: " << planner.visible_nodes.size() << "\n";
            std::cout << "  history depth: " << planner.navigation_history.size() << "\n";

        } else if(cmd == "plan.discuss"){
            // Interactive AI discussion about current plan node
            if(planner.current_path.empty()){
                std::cout << "⚠️  No current plan location. Use plan.goto <path> first.\n";
                result.success = false;
            } else {
                // Get current plan node
                auto node = vfs.tryResolveForOverlay(planner.current_path, cwd.primary_overlay);
                if(!node){
                    std::cout << "⚠️  Current plan path not found: " << planner.current_path << "\n";
                    result.success = false;
                } else {
                    // Build context from visible nodes
                    std::string context_str = "Current plan location: " + planner.current_path + "\n";
                    context_str += "Mode: " + std::string(planner.mode == PlannerContext::Mode::Forward ? "Forward (adding details)" : "Backward (revising)") + "\n\n";

                    // Add tag constraints if present
                    auto current_tags_ptr = vfs.tag_storage.getTags(node.get());
                    if(current_tags_ptr && !current_tags_ptr->empty()){
                        auto complete_tags = vfs.logic_engine.inferTags(*current_tags_ptr, 0.8f);
                        std::vector<std::string> tag_names;
                        for(auto tag_id : complete_tags){
                            tag_names.push_back(vfs.tag_registry.getTagName(tag_id));
                        }
                        context_str += "=== Tag Constraints ===\n";
                        context_str += "This plan has the following requirements/constraints: ";
                        for(size_t i = 0; i < tag_names.size(); ++i){
                            if(i > 0) context_str += ", ";
                            context_str += tag_names[i];
                        }
                        context_str += "\n";

                        // Check for conflicts
                        auto conflict = vfs.logic_engine.checkConsistency(complete_tags);
                        if(conflict){
                            context_str += "⚠️  WARNING: Tag conflict detected - " + conflict->description + "\n";
                            context_str += "Please help resolve this conflict before proceeding with planning.\n";
                        } else {
                            context_str += "✓ Tags are consistent - ensure new plans satisfy these constraints.\n";
                        }
                        context_str += "\n";
                    }

                    // Add current node content
                    context_str += "=== Current Node ===\n";
                    context_str += node->name + "\n";
                    if(!node->isDir()){
                        std::string content = node->read();
                        if(!content.empty()){
                            context_str += "Content:\n" + content + "\n";
                        }
                    }

                    // Add visible context nodes
                    if(!planner.visible_nodes.empty()){
                        context_str += "\n=== Context Nodes ===\n";
                        for(const auto& vpath : planner.visible_nodes){
                            auto ctx_node = vfs.tryResolveForOverlay(vpath, cwd.primary_overlay);
                            if(ctx_node){
                                context_str += "\n" + vpath + ":\n";
                                if(!ctx_node->isDir()){
                                    context_str += ctx_node->read() + "\n";
                                }
                            }
                        }
                    }

                    // Get user message
                    std::string user_msg;
                    bool enter_subloop = inv.args.empty();

                    if(!enter_subloop){
                        // Message provided as arguments - single execution
                        user_msg = inv.args[0];
                        for(size_t i = 1; i < inv.args.size(); ++i){
                            user_msg += " " + inv.args[i];
                        }
                    }

                    if(enter_subloop){
                        // Enter interactive plan.discuss sub-loop
                        std::cout << "💬 Entering interactive plan discussion mode. Type 'exit' or 'back' to return.\n";
                        std::cout << "📍 Context: " << planner.current_path << " (" <<
                            (planner.mode == PlannerContext::Mode::Forward ? "forward" : "backward") << ")\n";

                        std::string sub_line;
                        std::vector<std::string> sub_history;
                        while(true){
                            if(!read_line_with_history(vfs, "plan.discuss> ", sub_line, sub_history, cwd.path)){
                                break;
                            }
                            auto trimmed = trim_copy(sub_line);
                            if(trimmed.empty()) continue;
                            if(trimmed == "exit" || trimmed == "back" || trimmed == "quit"){
                                std::cout << "👋 Exiting plan.discuss mode\n";
                                break;
                            }

                            // Process as plan.discuss command with argument
                            CommandInvocation sub_inv;
                            sub_inv.name = "plan.discuss";
                            sub_inv.args.push_back(trimmed);
                            try{
                                auto sub_result = execute_single(sub_inv, "");
                                if(!sub_result.output.empty()){
                                    std::cout << sub_result.output;
                                    std::cout.flush();
                                }
                            } catch(const std::exception& e){
                                std::cout << "error: " << e.what() << "\n";
                            }
                        }
                    } else if(user_msg.empty()){
                        std::cout << "⚠️  No message provided\n";
                        result.success = false;
                    } else {
                        // Build prompt based on mode
                        std::string prompt;
                        if(planner.mode == PlannerContext::Mode::Forward){
                            prompt = context_str + "\n=== Task (Forward Mode) ===\n"
                                "Help add details to this plan. User says: " + user_msg + "\n\n"
                                "Guidelines:\n"
                                "- Break down high-level goals into concrete steps\n"
                                "- Suggest specific implementation approaches\n"
                                "- Use plan.create to add subplans, goals, jobs\n"
                                "- Ask clarifying questions if needed (format: Q: <question>)\n"
                                "- Use hypothesis.* commands to test assumptions\n"
                                "- IMPORTANT: Respect tag constraints - any suggestions must be compatible with listed tags\n"
                                "- Use plan.tags.check to verify consistency if adding new tags\n\n"
                                "Available commands: " + snippets::tool_list();
                        } else {
                            prompt = context_str + "\n=== Task (Backward Mode) ===\n"
                                "Review and revise higher-level plan. User says: " + user_msg + "\n\n"
                                "Guidelines:\n"
                                "- Identify issues with current strategy\n"
                                "- Suggest alternative approaches\n"
                                "- Update goals and strategy nodes if needed\n"
                                "- Ask clarifying questions (format: Q: <question>)\n"
                                "- Consider trade-offs and constraints\n"
                                "- IMPORTANT: Check if tag conflicts might be causing issues\n"
                                "- Use plan.verify to check tag consistency of proposed changes\n\n"
                                "Available commands: " + snippets::tool_list();
                        }

                        // Track in discussion session if active
                        if(discuss.is_active()){
                            discuss.add_message("user", "plan.discuss @ " + planner.current_path + ": " + user_msg);
                            discuss.current_plan_path = planner.current_path;
                        }

                        std::cout << "🤔 Thinking...\n";
                        result.output = call_ai(prompt);

                        if(discuss.is_active()){
                            discuss.add_message("assistant", result.output);
                        }

                        // Parse response for questions
                        auto lines = split_lines(result.output);
                        bool has_question = false;
                        for(const auto& line : lines.lines){
                            auto trimmed = trim_copy(line);
                            if(trimmed.size() > 2 && trimmed[0] == 'Q' && trimmed[1] == ':'){
                                has_question = true;
                                std::cout << "❓ " << trimmed.substr(2) << "\n";
                            }
                        }

                        if(has_question){
                            std::cout << "\n💡 Tip: Answer with 'yes', 'no', or 'explain <reason>' and call plan.discuss again\n";
                        }
                    }
                }
            }

        } else if(cmd == "plan.answer"){
            // Answer AI questions with yes/no/explain
            if(inv.args.empty()){
                std::cout << "plan.answer <yes|no|explain> [reason...]\n";
                result.success = false;
            } else {
                std::string answer_type = inv.args[0];
                std::transform(answer_type.begin(), answer_type.end(), answer_type.begin(), ::tolower);

                if(answer_type != "yes" && answer_type != "no" && answer_type != "explain"){
                    std::cout << "⚠️  Answer type must be 'yes', 'no', or 'explain'\n";
                    result.success = false;
                } else if(!discuss.is_active()){
                    std::cout << "⚠️  No active discussion session. Start with 'discuss' or 'plan.discuss' first.\n";
                    result.success = false;
                } else {
                    // Build answer message
                    std::string answer_msg;
                    if(answer_type == "yes"){
                        answer_msg = "Yes";
                        if(inv.args.size() > 1){
                            answer_msg += " - ";
                            for(size_t i = 1; i < inv.args.size(); ++i){
                                if(i > 1) answer_msg += " ";
                                answer_msg += inv.args[i];
                            }
                        }
                    } else if(answer_type == "no"){
                        answer_msg = "No";
                        if(inv.args.size() > 1){
                            answer_msg += " - ";
                            for(size_t i = 1; i < inv.args.size(); ++i){
                                if(i > 1) answer_msg += " ";
                                answer_msg += inv.args[i];
                            }
                        }
                    } else { // explain
                        if(inv.args.size() < 2){
                            std::cout << "⚠️  'explain' requires a reason\n";
                            result.success = false;
                        } else {
                            answer_msg = "Let me explain: ";
                            for(size_t i = 1; i < inv.args.size(); ++i){
                                if(i > 1) answer_msg += " ";
                                answer_msg += inv.args[i];
                            }
                        }
                    }

                    if(result.success){
                        // Add to conversation and get AI response
                        discuss.add_message("user", answer_msg);

                        // Build context with recent conversation
                        std::string context = "Previous conversation:\n";
                        size_t start_idx = discuss.conversation_history.size() >= 6 ? discuss.conversation_history.size() - 6 : 0;
                        for(size_t i = start_idx; i < discuss.conversation_history.size(); ++i){
                            context += discuss.conversation_history[i] + "\n";
                        }

                        context += "\nUser just answered: " + answer_msg + "\n";
                        context += "Continue the discussion based on this answer.\n";
                        if(planner.mode == PlannerContext::Mode::Forward){
                            context += "Mode: Forward (adding details to plans)\n";
                        } else {
                            context += "Mode: Backward (revising high-level plans)\n";
                        }
                        context += "Available commands: " + snippets::tool_list();

                        std::cout << "🤔 Processing your answer...\n";
                        result.output = call_ai(context);
                        discuss.add_message("assistant", result.output);

                        // Parse for more questions
                        auto lines = split_lines(result.output);
                        bool has_question = false;
                        for(const auto& line : lines.lines){
                            auto trimmed = trim_copy(line);
                            if(trimmed.size() > 2 && trimmed[0] == 'Q' && trimmed[1] == ':'){
                                has_question = true;
                                std::cout << "❓ " << trimmed.substr(2) << "\n";
                            }
                        }

                        if(has_question){
                            std::cout << "\n💡 Tip: Use 'plan.answer yes|no|explain <reason>' to respond\n";
                        }
                    }
                }
            }

        } else if(cmd == "plan.hypothesis"){
            // Generate hypothesis for current plan using existing hypothesis testing
            if(planner.current_path.empty()){
                std::cout << "⚠️  No current plan location. Use plan.goto <path> first.\n";
                result.success = false;
            } else {
                auto node = vfs.tryResolveForOverlay(planner.current_path, cwd.primary_overlay);
                if(!node){
                    std::cout << "⚠️  Current plan path not found: " + planner.current_path << "\n";
                    result.success = false;
                } else {
                    // Determine hypothesis type from args or plan content
                    std::string hyp_type = inv.args.empty() ? "" : inv.args[0];

                    // Read plan content to determine what to test
                    std::string plan_content = node->read();

                    std::cout << "🔬 Generating hypothesis for: " << planner.current_path << "\n";

                    // Auto-detect hypothesis type based on content keywords
                    if(hyp_type.empty()){
                        std::string lower_content = plan_content;
                        std::transform(lower_content.begin(), lower_content.end(), lower_content.begin(), ::tolower);

                        if(lower_content.find("error") != std::string::npos ||
                           lower_content.find("exception") != std::string::npos ||
                           lower_content.find("failure") != std::string::npos){
                            hyp_type = "errorhandling";
                        } else if(lower_content.find("duplicate") != std::string::npos ||
                                  lower_content.find("repeat") != std::string::npos ||
                                  lower_content.find("refactor") != std::string::npos){
                            hyp_type = "duplicates";
                        } else if(lower_content.find("log") != std::string::npos ||
                                  lower_content.find("trace") != std::string::npos ||
                                  lower_content.find("debug") != std::string::npos){
                            hyp_type = "logging";
                        } else if(lower_content.find("pattern") != std::string::npos ||
                                  lower_content.find("architecture") != std::string::npos ||
                                  lower_content.find("design") != std::string::npos){
                            hyp_type = "pattern";
                        } else {
                            hyp_type = "query";
                        }
                    }

                    std::cout << "📋 Detected type: " << hyp_type << "\n";

                    // Build hypothesis context with plan info
                    std::string goal;
                    std::string description = "Hypothesis generated from plan: " + planner.current_path;

                    if(hyp_type == "errorhandling"){
                        // Extract function names from plan content
                        goal = "Analyze error handling opportunities in the codebase";
                        std::cout << "💡 Suggestion: Use 'hypothesis.errorhandling <function>' to test specific functions\n";
                    } else if(hyp_type == "duplicates"){
                        goal = "Find duplicate code blocks that could be refactored";
                        std::cout << "💡 Suggestion: Use 'hypothesis.duplicates [path]' to scan for duplicates\n";
                    } else if(hyp_type == "logging"){
                        goal = "Plan logging instrumentation for error tracking";
                        std::cout << "💡 Suggestion: Use 'hypothesis.logging [path]' to identify logging points\n";
                    } else if(hyp_type == "pattern"){
                        goal = "Evaluate architectural patterns for the design";
                        std::cout << "💡 Suggestion: Use 'hypothesis.pattern <visitor|factory|singleton>' to evaluate patterns\n";
                    } else {
                        goal = "Query codebase for relevant patterns";
                        std::cout << "💡 Suggestion: Use 'hypothesis.query <target>' to search for specific elements\n";
                    }

                    // Add hypothesis node to plan tree if in planning mode
                    if(planner.mode == PlannerContext::Mode::Forward){
                        std::string hyp_name = "hypothesis_" + hyp_type;
                        auto hyp_node = std::make_shared<PlanResearch>(hyp_name);
                        hyp_node->content = "Type: " + hyp_type + "\nGoal: " + goal + "\nDescription: " + description;

                        auto parent = vfs.tryResolveForOverlay(planner.current_path, cwd.primary_overlay);
                        if(parent && parent->isDir()){
                            parent->children()[hyp_node->name] = hyp_node;
                            hyp_node->parent = parent;
                            std::string hyp_path = planner.current_path + "/" + hyp_name;
                            std::cout << "✅ Created hypothesis node at: " << hyp_path << "\n";
                            planner.addToContext(hyp_path);
                        }
                    }

                    std::cout << "\n📊 Hypothesis Goal: " << goal << "\n";
                    std::cout << "💡 Use the suggested hypothesis.* command to run the test\n";
                    std::cout << "💡 Then use 'plan.discuss' to incorporate findings into your plan\n";
                }
            }

        } else if(cmd == "plan.jobs.add"){
            if(inv.args.size() < 2) throw std::runtime_error("plan.jobs.add <jobs-path> <description> [priority] [assignee]");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            auto node = vfs.tryResolveForOverlay(vfs_path, cwd.primary_overlay);
            if(!node) throw std::runtime_error("plan.jobs.add: path not found: " + vfs_path);
            auto jobs_node = std::dynamic_pointer_cast<PlanJobs>(node);
            if(!jobs_node) throw std::runtime_error("plan.jobs.add: not a jobs node: " + vfs_path);

            std::string description = inv.args[1];
            int priority = 100;
            std::string assignee = "";
            if(inv.args.size() > 2){
                priority = std::stoi(inv.args[2]);
            }
            if(inv.args.size() > 3){
                assignee = inv.args[3];
            }

            jobs_node->addJob(description, priority, assignee);
            std::cout << "added job to " << vfs_path << "\n";

        } else if(cmd == "plan.jobs.complete"){
            if(inv.args.size() < 2) throw std::runtime_error("plan.jobs.complete <jobs-path> <index>");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            auto node = vfs.tryResolveForOverlay(vfs_path, cwd.primary_overlay);
            if(!node) throw std::runtime_error("plan.jobs.complete: path not found: " + vfs_path);
            auto jobs_node = std::dynamic_pointer_cast<PlanJobs>(node);
            if(!jobs_node) throw std::runtime_error("plan.jobs.complete: not a jobs node: " + vfs_path);

            size_t index = std::stoul(inv.args[1]);
            jobs_node->completeJob(index);
            std::cout << "marked job " << index << " as completed in " << vfs_path << "\n";

        } else if(cmd == "plan.verify"){
            // Verify tag consistency for a plan node
            std::string vfs_path = planner.current_path;
            if(!inv.args.empty()){
                vfs_path = normalize_path(cwd.path, inv.args[0]);
            }
            if(vfs_path.empty()){
                std::cout << "plan.verify: no path specified and no current plan location\n";
                result.success = false;
            } else {
                auto node = vfs.tryResolveForOverlay(vfs_path, cwd.primary_overlay);
                if(!node){
                    std::cout << "plan.verify: path not found: " << vfs_path << "\n";
                    result.success = false;
                } else {
                    // Collect all tags from this node
                    auto tags_ptr = vfs.tag_storage.getTags(node.get());
                    if(!tags_ptr || tags_ptr->empty()){
                        std::cout << "✓ No tags attached to " << vfs_path << "\n";
                    } else {
                        // Convert TagIds to names and show them
                        const TagSet& tags = *tags_ptr;
                        std::vector<std::string> tag_names;
                        for(auto tag_id : tags){
                            tag_names.push_back(vfs.tag_registry.getTagName(tag_id));
                        }
                        std::cout << "📋 Tags on " << vfs_path << ": ";
                        for(size_t i = 0; i < tag_names.size(); ++i){
                            if(i > 0) std::cout << ", ";
                            std::cout << tag_names[i];
                        }
                        std::cout << "\n";

                        // Check consistency
                        auto conflict = vfs.logic_engine.checkConsistency(tags);
                        if(conflict){
                            std::cout << "❌ Conflict detected: " << conflict->description << "\n";
                            if(!conflict->conflicting_tags.empty()){
                                std::cout << "   Conflicting tags: ";
                                for(size_t i = 0; i < conflict->conflicting_tags.size(); ++i){
                                    if(i > 0) std::cout << ", ";
                                    std::cout << conflict->conflicting_tags[i];
                                }
                                std::cout << "\n";
                            }
                            if(!conflict->suggestions.empty()){
                                std::cout << "   Suggestions: ";
                                for(size_t i = 0; i < conflict->suggestions.size(); ++i){
                                    if(i > 0) std::cout << " OR ";
                                    std::cout << conflict->suggestions[i];
                                }
                                std::cout << "\n";
                            }
                            result.success = false;
                        } else {
                            std::cout << "✓ Tag set is consistent (no conflicts detected)\n";
                        }
                    }
                }
            }

        } else if(cmd == "plan.tags.infer"){
            // Show complete inferred tag set for a plan node
            std::string vfs_path = planner.current_path;
            if(!inv.args.empty()){
                vfs_path = normalize_path(cwd.path, inv.args[0]);
            }
            if(vfs_path.empty()){
                std::cout << "plan.tags.infer: no path specified and no current plan location\n";
                result.success = false;
            } else {
                auto node = vfs.tryResolveForOverlay(vfs_path, cwd.primary_overlay);
                if(!node){
                    std::cout << "plan.tags.infer: path not found: " << vfs_path << "\n";
                    result.success = false;
                } else {
                    auto initial_tags_ptr = vfs.tag_storage.getTags(node.get());
                    if(!initial_tags_ptr || initial_tags_ptr->empty()){
                        std::cout << "📋 No initial tags on " << vfs_path << "\n";
                    } else {
                        const TagSet& initial_tags = *initial_tags_ptr;
                        // Show initial tags
                        std::vector<std::string> initial_names;
                        for(auto tag_id : initial_tags){
                            initial_names.push_back(vfs.tag_registry.getTagName(tag_id));
                        }
                        std::cout << "📋 Initial tags: ";
                        for(size_t i = 0; i < initial_names.size(); ++i){
                            if(i > 0) std::cout << ", ";
                            std::cout << initial_names[i];
                        }
                        std::cout << "\n";

                        // Infer complete tag set
                        auto complete_tags = vfs.logic_engine.inferTags(initial_tags, 0.8f);

                        // Find only the newly inferred tags
                        TagSet new_tags;
                        for(auto tag_id : complete_tags){
                            if(initial_tags.count(tag_id) == 0){
                                new_tags.insert(tag_id);
                            }
                        }

                        if(new_tags.empty()){
                            std::cout << "🔍 No additional tags inferred\n";
                        } else {
                            std::vector<std::string> new_names;
                            for(auto tag_id : new_tags){
                                new_names.push_back(vfs.tag_registry.getTagName(tag_id));
                            }
                            std::cout << "🔍 Inferred tags (only new): ";
                            for(size_t i = 0; i < new_names.size(); ++i){
                                if(i > 0) std::cout << ", ";
                                std::cout << new_names[i];
                            }
                            std::cout << "\n";
                        }

                        // Show complete tag set for planner use
                        std::vector<std::string> complete_names;
                        for(auto tag_id : complete_tags){
                            complete_names.push_back(vfs.tag_registry.getTagName(tag_id));
                        }
                        std::cout << "📦 Complete tag set (initial + inferred): ";
                        for(size_t i = 0; i < complete_names.size(); ++i){
                            if(i > 0) std::cout << ", ";
                            std::cout << complete_names[i];
                        }
                        std::cout << "\n";
                    }
                }
            }

        } else if(cmd == "plan.tags.check"){
            // Verify no conflicts in plan node's tag set (after inference)
            std::string vfs_path = planner.current_path;
            if(!inv.args.empty()){
                vfs_path = normalize_path(cwd.path, inv.args[0]);
            }
            if(vfs_path.empty()){
                std::cout << "plan.tags.check: no path specified and no current plan location\n";
                result.success = false;
            } else {
                auto node = vfs.tryResolveForOverlay(vfs_path, cwd.primary_overlay);
                if(!node){
                    std::cout << "plan.tags.check: path not found: " << vfs_path << "\n";
                    result.success = false;
                } else {
                    auto initial_tags_ptr = vfs.tag_storage.getTags(node.get());
                    if(!initial_tags_ptr || initial_tags_ptr->empty()){
                        std::cout << "✓ No tags to check on " << vfs_path << "\n";
                    } else {
                        const TagSet& initial_tags = *initial_tags_ptr;
                        // Infer complete tag set
                        auto complete_tags = vfs.logic_engine.inferTags(initial_tags, 0.8f);

                        // Check consistency of complete tag set
                        auto conflict = vfs.logic_engine.checkConsistency(complete_tags);
                        if(conflict){
                            std::cout << "❌ Conflict detected after tag inference: " << conflict->description << "\n";
                            if(!conflict->conflicting_tags.empty()){
                                std::cout << "   Conflicting tags: ";
                                for(size_t i = 0; i < conflict->conflicting_tags.size(); ++i){
                                    if(i > 0) std::cout << ", ";
                                    std::cout << conflict->conflicting_tags[i];
                                }
                                std::cout << "\n";
                            }
                            if(!conflict->suggestions.empty()){
                                std::cout << "   Suggestions: ";
                                for(size_t i = 0; i < conflict->suggestions.size(); ++i){
                                    if(i > 0) std::cout << " OR ";
                                    std::cout << conflict->suggestions[i];
                                }
                                std::cout << "\n";
                            }
                            std::cout << "💡 Use 'plan.tags.infer' to see the complete inferred tag set\n";
                            result.success = false;
                        } else {
                            std::cout << "✓ Complete tag set (after inference) is consistent\n";
                            // Show how many tags were inferred
                            size_t inferred_count = complete_tags.size() - initial_tags.size();
                            if(inferred_count > 0){
                                std::cout << "   (" << initial_tags.size() << " initial + "
                                         << inferred_count << " inferred = "
                                         << complete_tags.size() << " total tags)\n";
                            }
                        }
                    }
                }
            }

        } else if(cmd == "plan.validate"){
            // Recursively validate entire plan subtree for tag consistency
            std::string vfs_path = planner.current_path;
            if(!inv.args.empty()){
                vfs_path = normalize_path(cwd.path, inv.args[0]);
            }
            if(vfs_path.empty()){
                vfs_path = "/plan";  // Default to entire plan tree
            }

            auto root_node = vfs.tryResolveForOverlay(vfs_path, cwd.primary_overlay);
            if(!root_node){
                std::cout << "plan.validate: path not found: " << vfs_path << "\n";
                result.success = false;
            } else {
                std::cout << "🔍 Validating plan tree starting at: " << vfs_path << "\n\n";

                // Recursively check all nodes in the subtree
                struct ValidationResult {
                    std::string path;
                    bool has_conflict;
                    std::string conflict_desc;
                };
                std::vector<ValidationResult> results;
                int total_checked = 0;
                int total_with_tags = 0;
                int total_conflicts = 0;

                std::function<void(const std::string&, std::shared_ptr<VfsNode>)> validate_subtree;
                validate_subtree = [&](const std::string& path, std::shared_ptr<VfsNode> node){
                    total_checked++;

                    // Check tags on this node
                    auto tags_ptr = vfs.tag_storage.getTags(node.get());
                    if(tags_ptr && !tags_ptr->empty()){
                        total_with_tags++;
                        const TagSet& tags = *tags_ptr;
                        auto complete_tags = vfs.logic_engine.inferTags(tags, 0.8f);
                        auto conflict = vfs.logic_engine.checkConsistency(complete_tags);

                        ValidationResult vr;
                        vr.path = path;
                        vr.has_conflict = (conflict.has_value());
                        if(conflict){
                            vr.conflict_desc = conflict->description;
                            total_conflicts++;
                        }
                        results.push_back(vr);
                    }

                    // Recurse into children
                    if(node->isDir()){
                        auto& children = node->children();
                        for(const auto& [child_name, child_node] : children){
                            std::string child_path = path;
                            if(child_path.back() != '/') child_path += "/";
                            child_path += child_name;
                            validate_subtree(child_path, child_node);
                        }
                    }
                };

                validate_subtree(vfs_path, root_node);

                // Report results
                std::cout << "📊 Validation Summary:\n";
                std::cout << "   Total nodes checked: " << total_checked << "\n";
                std::cout << "   Nodes with tags: " << total_with_tags << "\n";
                std::cout << "   Tag conflicts found: " << total_conflicts << "\n\n";

                if(total_conflicts > 0){
                    std::cout << "❌ CONFLICTS DETECTED:\n\n";
                    for(const auto& vr : results){
                        if(vr.has_conflict){
                            std::cout << "  " << vr.path << "\n";
                            std::cout << "    ⚠️  " << vr.conflict_desc << "\n";
                            std::cout << "    💡 Use 'plan.verify " << vr.path << "' for details\n\n";
                        }
                    }
                    result.success = false;
                } else if(total_with_tags > 0){
                    std::cout << "✓ All plan nodes with tags are consistent!\n\n";
                    std::cout << "Nodes with tags:\n";
                    for(const auto& vr : results){
                        if(!vr.has_conflict){
                            std::cout << "  ✓ " << vr.path << "\n";
                        }
                    }
                } else {
                    std::cout << "ℹ️  No tags found in plan tree (nothing to validate)\n";
                }
            }

        } else if(cmd == "plan.save"){
            std::filesystem::path plan_file = "plan.vfs";
            if(!inv.args.empty()) plan_file = inv.args[0];

            try {
                if(plan_file.is_relative()) plan_file = std::filesystem::absolute(plan_file);

                // Create a temporary overlay to hold the /plan tree
                auto temp_root = std::make_shared<DirNode>("/");
                auto temp_overlay_id = vfs.registerOverlay("_plan_temp", temp_root);

                // Copy /plan tree content to the temporary overlay
                auto hits = vfs.resolveMulti("/plan");
                if(!hits.empty() && hits[0].node->isDir()){
                    // Clone the /plan directory structure by directly copying node references
                    std::function<void(const std::string&, std::shared_ptr<VfsNode>&)> clone_tree;
                    clone_tree = [&](const std::string& src_path, std::shared_ptr<VfsNode>& dst_parent){
                        auto listing = vfs.listDir(src_path, vfs.overlaysForPath(src_path));
                        for(const auto& [name, entry] : listing){
                            std::string child_path = src_path == "/" ? "/" + name : src_path + "/" + name;
                            if(!entry.nodes.empty()){
                                auto src_node = entry.nodes[0];
                                // Directly reference the source node (preserves type)
                                dst_parent->children()[name] = src_node;
                                // If it has children, continue cloning
                                if(src_node->isDir()){
                                    clone_tree(child_path, src_node);
                                }
                            }
                        }
                    };

                    auto plan_dir = std::make_shared<DirNode>("plan");
                    temp_root->children()["plan"] = plan_dir;
                    std::shared_ptr<VfsNode> plan_node = plan_dir;
                    clone_tree("/plan", plan_node);
                }

                save_overlay_to_file(vfs, temp_overlay_id, plan_file.string());
                vfs.unregisterOverlay(temp_overlay_id);
                std::cout << "saved plan tree to " << plan_file.string() << "\n";
            } catch(const std::exception& e){
                std::cout << "error saving plan: " << e.what() << "\n";
                result.success = false;
            }

        } else if(cmd == "solution.save"){
            std::filesystem::path target = solution.file_path;
            if(!inv.args.empty()) target = inv.args[0];
            if(!solution.active){
                std::cout << "no solution loaded\n";
                result.success = false;
            } else {
                if(target.empty()){
                    std::cout << "solution.save requires a file path\n";
                    result.success = false;
                } else {
                    try{
                        if(target.is_relative()) target = std::filesystem::absolute(target);
                    } catch(const std::exception& e){
                        std::cout << "error: solution.save: " << e.what() << "\n";
                        result.success = false;
                        target.clear();
                    }
                    if(!target.empty()){
                        solution.file_path = target.string();
                        if(!solution_save(vfs, solution, false)){
                            result.success = false;
                        }
                    }
                }
            }

        //
        // Action Planner commands
        //
        } else if(cmd == "context.build.adv" || cmd == "context.build.advanced"){
            // Build AI context with advanced options
            // Usage: context.build.adv [max_tokens] [--deps] [--dedup] [--summary=N] [--hierarchical] [--adaptive]
            size_t max_tokens = 4000;
            ContextBuilder::ContextOptions opts;

            for(size_t i = 0; i < inv.args.size(); ++i){
                const std::string& arg = inv.args[i];
                if(arg == "--deps"){
                    opts.include_dependencies = true;
                } else if(arg == "--dedup"){
                    opts.deduplicate = true;
                } else if(arg == "--hierarchical"){
                    opts.hierarchical = true;
                } else if(arg == "--adaptive"){
                    opts.adaptive_budget = true;
                } else if(arg.rfind("--summary=", 0) == 0){
                    opts.summary_threshold = std::stoi(arg.substr(10));
                } else if(i == 0){
                    max_tokens = std::stoul(arg);
                }
            }

            ContextBuilder builder(vfs, vfs.tag_storage, vfs.tag_registry, max_tokens);
            builder.collect();
            std::string context = builder.buildWithOptions(opts);

            std::cout << "=== Advanced Context Builder Results ===\n";
            std::cout << "Entries: " << builder.entryCount() << "\n";
            std::cout << "Total tokens: " << builder.totalTokens() << "\n";
            std::cout << "Options: deps=" << opts.include_dependencies
                     << " dedup=" << opts.deduplicate
                     << " hierarchical=" << opts.hierarchical
                     << " adaptive=" << opts.adaptive_budget
                     << " summary_thresh=" << opts.summary_threshold << "\n";
            std::cout << "Context (first 500 chars):\n";
            std::cout << context.substr(0, std::min(size_t(500), context.size())) << "\n";
            if(context.size() > 500) std::cout << "... (truncated)\n";

        } else if(cmd == "context.build"){
            // Build AI context from VFS with filters
            // Usage: context.build [max_tokens]
            size_t max_tokens = 4000;
            if(!inv.args.empty()){
                max_tokens = std::stoul(inv.args[0]);
            }

            ContextBuilder builder(vfs, vfs.tag_storage, vfs.tag_registry, max_tokens);
            builder.collect();
            std::string context = builder.buildWithPriority();

            std::cout << "=== Context Builder Results ===\n";
            std::cout << "Entries: " << builder.entryCount() << "\n";
            std::cout << "Total tokens: " << builder.totalTokens() << "\n";
            std::cout << "Context (first 500 chars):\n";
            std::cout << context.substr(0, std::min(size_t(500), context.size())) << "\n";
            if(context.size() > 500) std::cout << "... (truncated)\n";

        } else if(cmd == "context.filter.tag"){
            // Add tag-based filter to context builder
            // Usage: context.filter.tag <tag-name> [mode: any|all|none]
            if(inv.args.empty()) throw std::runtime_error("context.filter.tag <tag-name> [any|all|none]");

            std::string tag_name = inv.args[0];
            std::string mode = inv.args.size() > 1 ? inv.args[1] : "any";

            TagId tag_id = vfs.getTagId(tag_name);
            if(tag_id == TAG_INVALID){
                throw std::runtime_error("unknown tag: " + tag_name);
            }

            std::cout << "Filter configured: tag=" << tag_name << " mode=" << mode << "\n";
            std::cout << "(Use context.build to apply filters)\n";

        } else if(cmd == "context.filter.path"){
            // Add path-based filter
            // Usage: context.filter.path <prefix-or-pattern>
            if(inv.args.empty()) throw std::runtime_error("context.filter.path <prefix-or-pattern>");

            std::string pattern = inv.args[0];
            std::cout << "Filter configured: path pattern=" << pattern << "\n";
            std::cout << "(Use context.build to apply filters)\n";

        } else if(cmd == "test.planner"){
            // Run action planner test suite
            // Usage: test.planner
            ActionPlannerTestSuite suite(vfs, vfs.tag_storage, vfs.tag_registry);

            // Test 1: Tag-based filtering
            suite.addTest("tag_filter_any", "Test TagAny filter", [&](){
                // Create test nodes with tags
                auto test_file = std::make_shared<FileNode>("test1.txt", "test content");
                vfs_add(vfs, "/test/file1.txt", test_file, cwd.primary_overlay);

                TagId test_tag = vfs.registerTag("test-tag");
                vfs.tag_storage.addTag(test_file.get(), test_tag);

                // Test filter
                TagSet tags{test_tag};
                ContextFilter filter = ContextFilter::tagAny(tags);
                return filter.matches(test_file.get(), "/test/file1.txt", vfs);
            });

            // Test 2: Path prefix filtering
            suite.addTest("path_prefix", "Test path prefix filter", [&](){
                auto test_file = std::make_shared<FileNode>("test2.txt", "test content");
                vfs_add(vfs, "/cpp/test2.txt", test_file, cwd.primary_overlay);

                ContextFilter filter = ContextFilter::pathPrefix("/cpp/");
                return filter.matches(test_file.get(), "/cpp/test2.txt", vfs);
            });

            // Test 3: Content matching
            suite.addTest("content_match", "Test content matching filter", [&](){
                auto test_file = std::make_shared<FileNode>("test3.txt", "hello world");
                vfs_add(vfs, "/test/file3.txt", test_file, cwd.primary_overlay);

                ContextFilter filter = ContextFilter::contentMatch("hello");
                return filter.matches(test_file.get(), "/test/file3.txt", vfs);
            });

            // Test 4: Context builder with token limits
            suite.addTest("context_builder_tokens", "Test context builder token limiting", [&](){
                auto test_file = std::make_shared<FileNode>("large.txt", std::string(10000, 'x'));
                vfs_add(vfs, "/test/large.txt", test_file, cwd.primary_overlay);

                ContextBuilder builder(vfs, vfs.tag_storage, vfs.tag_registry, 1000);
                builder.collect();
                return builder.totalTokens() > 0;
            });

            // Test 5: Replacement strategy - replace all
            suite.addTest("replacement_all", "Test replaceAll strategy", [&](){
                auto test_file = std::make_shared<FileNode>("replace.txt", "old content");
                vfs_add(vfs, "/test/replace.txt", test_file, cwd.primary_overlay);

                ReplacementStrategy strategy = ReplacementStrategy::replaceAll("/test/replace.txt", "new content");
                bool success = strategy.apply(vfs);
                std::string new_content = test_file->read();
                return success && new_content == "new content";
            });

            // Test 6: Replacement strategy - insert before
            suite.addTest("replacement_insert_before", "Test insertBefore strategy", [&](){
                auto test_file = std::make_shared<FileNode>("insert.txt", "line1\ntarget\nline3");
                vfs_add(vfs, "/test/insert.txt", test_file, cwd.primary_overlay);

                ReplacementStrategy strategy = ReplacementStrategy::insertBefore("/test/insert.txt", "target", "inserted");
                bool success = strategy.apply(vfs);
                std::string new_content = test_file->read();
                return success && new_content.find("inserted") != std::string::npos;
            });

            // Run all tests
            suite.runAll();
            suite.printResults();

        } else if(cmd == "test.hypothesis"){
            // Run hypothesis test suite
            // Usage: test.hypothesis
            HypothesisTestSuite suite(vfs, vfs.tag_storage, vfs.tag_registry);
            suite.createStandardSuite();
            suite.runAll();
            suite.printResults();

        } else if(cmd == "hypothesis.test"){
            // Test a specific hypothesis
            // Usage: hypothesis.test <level> <goal> [description]
            if(inv.args.empty()) throw std::runtime_error("hypothesis.test <level> <goal> [description]");

            int level_num = std::stoi(inv.args[0]);
            if(level_num < 1 || level_num > 5){
                throw std::runtime_error("Level must be 1-5");
            }

            std::string goal = inv.args.size() > 1 ? inv.args[1] : "";
            std::string desc = inv.args.size() > 2 ? join_args(inv.args, 2) : "Custom hypothesis";

            Hypothesis::Level level = static_cast<Hypothesis::Level>(level_num);
            Hypothesis hyp(level, desc, goal);

            HypothesisTester tester(vfs, vfs.tag_storage, vfs.tag_registry);
            auto result = tester.test(hyp);

            std::cout << "\n=== " << hyp.levelName() << " ===\n";
            std::cout << "Description: " << hyp.description << "\n";
            std::cout << "Goal: " << hyp.goal << "\n";
            std::cout << result.summary();

        } else if(cmd == "hypothesis.query"){
            // Level 1: Simple query - find pattern in VFS
            // Usage: hypothesis.query <target> [search_path]
            if(inv.args.empty()) throw std::runtime_error("hypothesis.query <target> [search_path]");

            std::string target = inv.args[0];
            std::string search_path = inv.args.size() > 1 ? inv.args[1] : "/";

            HypothesisTester tester(vfs, vfs.tag_storage, vfs.tag_registry);
            auto result = tester.testSimpleQuery(target, search_path);

            std::cout << "\n=== Level 1: Simple Query ===\n";
            std::cout << result.summary();

        } else if(cmd == "hypothesis.errorhandling"){
            // Level 2: Error handling addition
            // Usage: hypothesis.errorhandling <function_name> [style]
            if(inv.args.empty()) throw std::runtime_error("hypothesis.errorhandling <function_name> [style]");

            std::string function_name = inv.args[0];
            std::string style = inv.args.size() > 1 ? inv.args[1] : "try-catch";

            HypothesisTester tester(vfs, vfs.tag_storage, vfs.tag_registry);
            auto result = tester.testErrorHandlingAddition(function_name, style);

            std::cout << "\n=== Level 2: Error Handling Addition ===\n";
            std::cout << result.summary();

        } else if(cmd == "hypothesis.duplicates"){
            // Level 3: Find duplicate code blocks
            // Usage: hypothesis.duplicates [search_path] [min_lines]
            std::string search_path = inv.args.size() > 0 ? inv.args[0] : "/";
            size_t min_lines = inv.args.size() > 1 ? std::stoul(inv.args[1]) : 3;

            HypothesisTester tester(vfs, vfs.tag_storage, vfs.tag_registry);
            auto result = tester.testDuplicateExtraction(search_path, min_lines);

            std::cout << "\n=== Level 3: Duplicate Code Detection ===\n";
            std::cout << result.summary();

        } else if(cmd == "hypothesis.logging"){
            // Level 4: Logging instrumentation planning
            // Usage: hypothesis.logging [search_path]
            std::string search_path = inv.args.size() > 0 ? inv.args[0] : "/";

            HypothesisTester tester(vfs, vfs.tag_storage, vfs.tag_registry);
            auto result = tester.testLoggingInstrumentation(search_path);

            std::cout << "\n=== Level 4: Logging Instrumentation ===\n";
            std::cout << result.summary();

        } else if(cmd == "hypothesis.pattern"){
            // Level 5: Architecture pattern evaluation
            // Usage: hypothesis.pattern <pattern_name> [target_path]
            if(inv.args.empty()) throw std::runtime_error("hypothesis.pattern <pattern_name> [target_path]");

            std::string pattern_name = inv.args[0];
            std::string target_path = inv.args.size() > 1 ? inv.args[1] : "/";

            HypothesisTester tester(vfs, vfs.tag_storage, vfs.tag_registry);
            auto result = tester.testArchitecturePattern(pattern_name, target_path);

            std::cout << "\n=== Level 5: Architecture Pattern Evaluation ===\n";
            std::cout << result.summary();

        } else if(cmd == "feedback.metrics.show"){
            // Show metrics history
            // Usage: feedback.metrics.show [top_n]
            if(!G_METRICS_COLLECTOR) throw std::runtime_error("metrics collector not initialized");

            size_t top_n = inv.args.size() > 0 ? std::stoul(inv.args[0]) : 10;

            std::cout << "\n=== Planner Metrics Summary ===\n";
            std::cout << "Total runs: " << G_METRICS_COLLECTOR->history.size() << "\n";
            std::cout << "Average success rate: " << (G_METRICS_COLLECTOR->getAverageSuccessRate() * 100) << "%\n";
            std::cout << "Average iterations: " << G_METRICS_COLLECTOR->getAverageIterations() << "\n\n";

            std::cout << "Top " << top_n << " most triggered rules:\n";
            auto triggered = G_METRICS_COLLECTOR->getMostTriggeredRules(top_n);
            for(size_t i = 0; i < triggered.size(); ++i){
                std::cout << "  " << (i+1) << ". " << triggered[i] << "\n";
            }

            std::cout << "\nTop " << top_n << " most failed rules:\n";
            auto failed = G_METRICS_COLLECTOR->getMostFailedRules(top_n);
            for(size_t i = 0; i < failed.size(); ++i){
                std::cout << "  " << (i+1) << ". " << failed[i] << "\n";
            }

        } else if(cmd == "feedback.metrics.save"){
            // Save metrics to VFS
            // Usage: feedback.metrics.save [path]
            if(!G_METRICS_COLLECTOR) throw std::runtime_error("metrics collector not initialized");

            std::string path = inv.args.size() > 0 ? inv.args[0] : "/plan/metrics";
            G_METRICS_COLLECTOR->saveToVfs(vfs, path);
            std::cout << "Metrics saved to " << path << "\n";

        } else if(cmd == "feedback.patches.list"){
            // List pending patches
            // Usage: feedback.patches.list
            if(!G_PATCH_STAGING) throw std::runtime_error("patch staging not initialized");

            auto pending = G_PATCH_STAGING->listPending();
            std::cout << "\n=== Pending Rule Patches (" << pending.size() << ") ===\n";
            for(size_t i = 0; i < pending.size(); ++i){
                const auto& patch = pending[i];
                std::cout << "[" << i << "] " << patch.rule_name << "\n";
                std::cout << "    Operation: ";
                switch(patch.operation){
                    case RulePatch::Operation::Add: std::cout << "Add"; break;
                    case RulePatch::Operation::Modify: std::cout << "Modify"; break;
                    case RulePatch::Operation::Remove: std::cout << "Remove"; break;
                    case RulePatch::Operation::AdjustConfidence: std::cout << "Adjust Confidence"; break;
                }
                std::cout << "\n";
                std::cout << "    Confidence: " << patch.new_confidence << "\n";
                std::cout << "    Source: " << patch.source << "\n";
                std::cout << "    Rationale: " << patch.rationale << "\n";
                std::cout << "    Evidence: " << patch.evidence_count << " runs\n\n";
            }

        } else if(cmd == "feedback.patches.apply"){
            // Apply a specific patch or all patches
            // Usage: feedback.patches.apply [index|all]
            if(!G_PATCH_STAGING) throw std::runtime_error("patch staging not initialized");

            if(inv.args.empty() || inv.args[0] == "all"){
                bool success = G_PATCH_STAGING->applyAll();
                if(success){
                    std::cout << "All patches applied successfully\n";
                } else {
                    std::cout << "Some patches failed to apply\n";
                }
            } else {
                size_t index = std::stoul(inv.args[0]);
                bool success = G_PATCH_STAGING->applyPatch(index);
                if(success){
                    std::cout << "Patch " << index << " applied successfully\n";
                } else {
                    std::cout << "Failed to apply patch " << index << "\n";
                }
            }

        } else if(cmd == "feedback.patches.reject"){
            // Reject a specific patch or all patches
            // Usage: feedback.patches.reject [index|all]
            if(!G_PATCH_STAGING) throw std::runtime_error("patch staging not initialized");

            if(inv.args.empty() || inv.args[0] == "all"){
                G_PATCH_STAGING->rejectAll();
                std::cout << "All patches rejected\n";
            } else {
                size_t index = std::stoul(inv.args[0]);
                bool success = G_PATCH_STAGING->rejectPatch(index, "User rejected");
                if(success){
                    std::cout << "Patch " << index << " rejected\n";
                } else {
                    std::cout << "Failed to reject patch " << index << "\n";
                }
            }

        } else if(cmd == "feedback.patches.save"){
            // Save patches to VFS
            // Usage: feedback.patches.save [path]
            if(!G_PATCH_STAGING) throw std::runtime_error("patch staging not initialized");

            std::string path = inv.args.size() > 0 ? inv.args[0] : "/plan/patches";
            G_PATCH_STAGING->savePendingToVfs(vfs, path + "/pending");
            G_PATCH_STAGING->saveAppliedToVfs(vfs, path + "/applied");
            std::cout << "Patches saved to " << path << "\n";

        } else if(cmd == "feedback.cycle"){
            // Run a full feedback cycle
            // Usage: feedback.cycle [--auto-apply] [--min-evidence=N]
            if(!G_FEEDBACK_LOOP) throw std::runtime_error("feedback loop not initialized");

            bool auto_apply = false;
            size_t min_evidence = 3;

            for(const auto& arg : inv.args){
                if(arg == "--auto-apply"){
                    auto_apply = true;
                } else if(arg.substr(0, 15) == "--min-evidence="){
                    min_evidence = std::stoul(arg.substr(15));
                }
            }

            auto result = G_FEEDBACK_LOOP->runCycle(auto_apply, min_evidence);
            std::cout << result.summary;

        } else if(cmd == "feedback.review"){
            // Interactive patch review
            // Usage: feedback.review
            if(!G_FEEDBACK_LOOP) throw std::runtime_error("feedback loop not initialized");

            G_FEEDBACK_LOOP->interactiveReview();

        } else if(cmd == "cpp.tu"){
            if(inv.args.empty()) throw std::runtime_error("cpp.tu <path>");
            std::string abs = normalize_path(cwd.path, inv.args[0]);
            auto tu = std::make_shared<CppTranslationUnit>(path_basename(abs));
            vfs_add(vfs, abs, tu, cwd.primary_overlay);
            std::cout << "cpp tu @ " << abs << "\n";

        } else if(cmd == "cpp.include"){
            if(inv.args.size() < 2) throw std::runtime_error("cpp.include <tu> <header> [angled]");
            std::string absTu = normalize_path(cwd.path, inv.args[0]);
            auto tu = expect_tu(vfs.resolveForOverlay(absTu, cwd.primary_overlay));
            int angled = 0;
            if(inv.args.size() > 2){
                try{
                    angled = std::stoi(inv.args[2]);
                } catch(const std::exception&){
                    angled = 0;
                }
            }
            auto inc = std::make_shared<CppInclude>("include", inv.args[1], angled != 0);
            tu->includes.push_back(inc);
            std::cout << "+include " << inv.args[1] << "\n";

        } else if(cmd == "cpp.func"){
            if(inv.args.size() < 3) throw std::runtime_error("cpp.func <tu> <name> <ret>");
            std::string absTu = normalize_path(cwd.path, inv.args[0]);
            auto tu = expect_tu(vfs.resolveForOverlay(absTu, cwd.primary_overlay));
            auto fn = std::make_shared<CppFunction>(inv.args[1], inv.args[2], inv.args[1]);
            std::string fnPath = join_path(absTu, inv.args[1]);
            vfs_add(vfs, fnPath, fn, cwd.primary_overlay);
            tu->funcs.push_back(fn);
            vfs_add(vfs, join_path(fnPath, "body"), fn->body, cwd.primary_overlay);
            std::cout << "+func " << inv.args[1] << "\n";

        } else if(cmd == "cpp.param"){
            if(inv.args.size() < 3) throw std::runtime_error("cpp.param <fn> <type> <name>");
            auto fn = expect_fn(vfs.resolveForOverlay(normalize_path(cwd.path, inv.args[0]), cwd.primary_overlay));
            fn->params.push_back(CppParam{inv.args[1], inv.args[2]});
            std::cout << "+param " << inv.args[1] << " " << inv.args[2] << "\n";

        } else if(cmd == "cpp.print"){
            if(inv.args.empty()) throw std::runtime_error("cpp.print <scope> <text>");
            auto block = expect_block(vfs.resolveForOverlay(normalize_path(cwd.path, inv.args[0]), cwd.primary_overlay));
            std::string text = unescape_meta(join_args(inv.args, 1));
            auto s = std::make_shared<CppString>("s", text);
            auto chain = std::vector<std::shared_ptr<CppExpr>>{ s, std::make_shared<CppId>("endl", "std::endl") };
            auto coutline = std::make_shared<CppStreamOut>("cout", chain);
            block->stmts.push_back(std::make_shared<CppExprStmt>("es", coutline));
            std::cout << "+print '" << text << "'\n";

        } else if(cmd == "cpp.returni"){
            if(inv.args.size() < 2) throw std::runtime_error("cpp.returni <scope> <int>");
            auto block = expect_block(vfs.resolveForOverlay(normalize_path(cwd.path, inv.args[0]), cwd.primary_overlay));
            long long value = std::stoll(inv.args[1]);
            block->stmts.push_back(std::make_shared<CppReturn>("ret", std::make_shared<CppInt>("i", value)));
            std::cout << "+return " << value << "\n";

        } else if(cmd == "cpp.return"){
            if(inv.args.empty()) throw std::runtime_error("cpp.return <scope> [expr]");
            auto block = expect_block(vfs.resolveForOverlay(normalize_path(cwd.path, inv.args[0]), cwd.primary_overlay));
            std::string trimmed = unescape_meta(trim_copy(join_args(inv.args, 1)));
            std::shared_ptr<CppExpr> expr;
            if(!trimmed.empty()) expr = std::make_shared<CppRawExpr>("rexpr", trimmed);
            block->stmts.push_back(std::make_shared<CppReturn>("ret", expr));
            std::cout << "+return expr\n";

        } else if(cmd == "cpp.expr"){
            if(inv.args.empty()) throw std::runtime_error("cpp.expr <scope> <expr>");
            auto block = expect_block(vfs.resolveForOverlay(normalize_path(cwd.path, inv.args[0]), cwd.primary_overlay));
            block->stmts.push_back(std::make_shared<CppExprStmt>("expr", std::make_shared<CppRawExpr>("rexpr", unescape_meta(join_args(inv.args, 1)))));
            std::cout << "+expr " << inv.args[0] << "\n";

        } else if(cmd == "cpp.vardecl"){
            if(inv.args.size() < 3) throw std::runtime_error("cpp.vardecl <scope> <type> <name> [init]");
            auto block = expect_block(vfs.resolveForOverlay(normalize_path(cwd.path, inv.args[0]), cwd.primary_overlay));
            std::string init = unescape_meta(trim_copy(join_args(inv.args, 3)));
            bool hasInit = !init.empty();
            block->stmts.push_back(std::make_shared<CppVarDecl>("var", inv.args[1], inv.args[2], init, hasInit));
            std::cout << "+vardecl " << inv.args[1] << " " << inv.args[2] << "\n";

        } else if(cmd == "cpp.stmt"){
            if(inv.args.empty()) throw std::runtime_error("cpp.stmt <scope> <stmt>");
            auto block = expect_block(vfs.resolveForOverlay(normalize_path(cwd.path, inv.args[0]), cwd.primary_overlay));
            block->stmts.push_back(std::make_shared<CppRawStmt>("stmt", unescape_meta(join_args(inv.args, 1))));
            std::cout << "+stmt " << inv.args[0] << "\n";

        } else if(cmd == "cpp.rangefor"){
            if(inv.args.size() < 2) throw std::runtime_error("cpp.rangefor <scope> <loop> decl | range");
            std::string rest = trim_copy(join_args(inv.args, 2));
            auto bar = rest.find('|');
            if(bar == std::string::npos) throw std::runtime_error("cpp.rangefor expects 'decl | range'");
            std::string decl = unescape_meta(trim_copy(rest.substr(0, bar)));
            std::string range = unescape_meta(trim_copy(rest.substr(bar + 1)));
            if(decl.empty() || range.empty()) throw std::runtime_error("cpp.rangefor missing decl or range");
            std::string absScope = normalize_path(cwd.path, inv.args[0]);
            auto block = expect_block(vfs.resolveForOverlay(absScope, cwd.primary_overlay));
            auto loop = std::make_shared<CppRangeFor>(inv.args[1], decl, range);
            block->stmts.push_back(loop);
            std::string loopPath = join_path(absScope, inv.args[1]);
            vfs_add(vfs, loopPath, loop, cwd.primary_overlay);
            vfs_add(vfs, join_path(loopPath, "body"), loop->body, cwd.primary_overlay);
            std::cout << "+rangefor " << inv.args[1] << "\n";

        } else if(cmd == "cpp.dump"){
            if(inv.args.size() < 2) throw std::runtime_error("cpp.dump <tu> <out>");
            std::string absTu = normalize_path(cwd.path, inv.args[0]);
            std::string absOut = normalize_path(cwd.path, inv.args[1]);
            cpp_dump_to_vfs(vfs, cwd.primary_overlay, absTu, absOut);
            std::cout << "dump -> " << absOut << "\n";

        } else if(cmd == "qwen"){
            // Interactive AI assistant powered by qwen-code
            QwenCmd::cmd_qwen(inv.args, vfs);

        } else if(cmd == "upp.asm.load"){
            // Parse flags first
            bool use_host_path = false;  // default behavior: treat as VFS path without -H flag
            std::string path_arg;
            
            for(const auto& arg : inv.args) {
                if(arg == "-H") {
                    use_host_path = true;  // with -H flag: treat as host OS path
                } else if(arg != "-H") {
                    path_arg = arg;  // store the actual path argument
                }
            }
            
            if(path_arg.empty()) throw std::runtime_error("upp.asm.load <var-file-path> [-H] (use -H for host OS path)");
            
            // If -H flag is provided, use host OS path behavior
            if(use_host_path) {
                std::string host_path = path_arg;
                
                // Extract directory and filename
                size_t last_slash = host_path.find_last_of("/\\");
                std::string host_dir, var_filename;
                if (last_slash != std::string::npos) {
                    host_dir = host_path.substr(0, last_slash);
                    var_filename = host_path.substr(last_slash + 1);
                } else {
                    host_dir = ".";  // Current directory if no path separators
                    var_filename = host_path;
                }
                
                // Check if the host file exists before attempting to mount
                std::ifstream test_file(host_path);
                if (!test_file.good()) {
                    throw std::runtime_error("Host file does not exist or is not accessible: " + host_path);
                }
                test_file.close();
                
                // Mount the directory containing the .var file to the VFS
                std::string vfs_mount_point = "/mnt/host_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(getpid());
                
                try {
                    vfs.mountFilesystem(host_dir, vfs_mount_point, cwd.primary_overlay);  // Use same overlay as shell
                } catch (const std::exception& e) {
                    throw std::runtime_error("Failed to mount host directory: " + host_dir + " - Exception: " + e.what());
                } catch (...) {
                    throw std::runtime_error("Failed to mount host directory: " + host_dir + " - Unknown exception");
                }
                
                // Give the mount system time to initialize
                usleep(300000); // 300ms delay
                
                // Load the .var file from the mounted location
                std::string var_vfs_path = vfs_mount_point + "/" + var_filename;
                
                // Read the .var file content from VFS
                std::string var_content;
                try {
                    var_content = vfs.read(var_vfs_path, std::nullopt);
                } catch (const std::exception& e) {
                    throw std::runtime_error("Failed to read .var file from mounted location. Expected at: " + var_vfs_path + " (from host: " + host_path + ") - Exception: " + e.what());
                } catch (...) {
                    throw std::runtime_error("Failed to read .var file from mounted location. Expected at: " + var_vfs_path + " (from host: " + host_path + ") - Unknown exception");
                }
                
                // Create UppAssembly instance and load the .var file
                auto assembly = std::make_shared<UppAssembly>();
                if(assembly->load_from_content(var_content, var_vfs_path)) {
                    std::cout << "Loaded U++ assembly from host: " << host_path << "\n";
                    
                    // Add to VFS structure
                    assembly->create_vfs_structure(vfs, cwd.primary_overlay);
                } else {
                    throw std::runtime_error("Failed to parse U++ assembly: " + host_path);
                }
            } else {
                // Without -H flag: treat as VFS path, load directly
                std::string var_path = normalize_path(cwd.path, path_arg);
                
                // Read the .var file content from VFS
                std::string var_content = vfs.read(var_path, std::nullopt);
                
                // Create UppAssembly instance and load the .var file
                auto assembly = std::make_shared<UppAssembly>();
                if(assembly->load_from_content(var_content, var_path)) {
                    std::cout << "Loaded U++ assembly: " << var_path << "\n";
                    
                    // Add to VFS structure
                    assembly->create_vfs_structure(vfs, cwd.primary_overlay);
                } else {
                    throw std::runtime_error("Failed to load U++ assembly: " + var_path);
                }
            }

        } else if(cmd == "upp.asm.create"){
            if(inv.args.size() < 2) throw std::runtime_error("upp.asm.create <name> <output-path>");
            std::string name = inv.args[0];
            std::string output_path = normalize_path(cwd.path, inv.args[1]);
            
            // Create an empty UppAssembly with the given name
            auto assembly = std::make_shared<UppAssembly>();
            auto workspace = std::make_shared<UppWorkspace>(name, output_path);
            
            // Add a default primary package
            auto primary_package = std::make_shared<UppPackage>(name, name + "/src", true);
            workspace->add_package(primary_package);
            
            // Set the workspace in the assembly
            assembly->set_workspace(workspace);
            
            // Save the assembly to the specified output path
            if(assembly->save_to_file(output_path)) {
                std::cout << "Created U++ assembly: " << output_path << "\n";
                
                // Add to VFS structure
                assembly->create_vfs_structure(vfs, cwd.primary_overlay);
            } else {
                throw std::runtime_error("Failed to create U++ assembly: " + output_path);
            }

        } else if(cmd == "upp.asm.list"){
            // Parse flags first
            bool verbose = false;
            for(const auto& arg : inv.args) {
                if(arg == "-v") {
                    verbose = true;
                }
            }
            
            // List packages in the current assembly
            std::cout << "U++ Assembly packages:\n";
            if(g_current_assembly) {
                // Get the workspace from the current assembly
                auto workspace = g_current_assembly->get_workspace();
                if(workspace) {
                    // Get the UPP paths to determine which to show
                    std::vector<std::string> search_paths;
                    
                    // Try to get the UPP paths from the .var file content
                    try {
                        // Look for UPP= line in the .var file
                        std::string var_file_path = workspace->assembly_path;
                        if (!var_file_path.empty()) {
                            std::string var_content = vfs.read(var_file_path, std::nullopt);
                            std::istringstream stream(var_content);
                            std::string line;
                            
                            while (std::getline(stream, line)) {
                                // Look for UPP= line (handle both "UPP=" and "UPP =" formats)
                                if (line.length() >= 3 && line.substr(0, 3) == "UPP") {
                                    // Find the equals sign
                                    size_t eq_pos = line.find('=');
                                    if (eq_pos != std::string::npos) {
                                        // Extract the paths from UPP = "path1;path2;..." or UPP="path1;path2;..."
                                        size_t start_quote = line.find('"', eq_pos);
                                        size_t end_quote = line.rfind('"');
                                        if (start_quote != std::string::npos && end_quote != std::string::npos && end_quote > start_quote) {
                                            std::string paths_str = line.substr(start_quote + 1, end_quote - start_quote - 1);
                                            
                                            // Split paths by semicolon
                                            size_t pos = 0;
                                            std::string delimiter = ";";
                                            while ((pos = paths_str.find(delimiter)) != std::string::npos) {
                                                std::string path = paths_str.substr(0, pos);
                                                // Remove leading/trailing whitespace
                                                path.erase(0, path.find_first_not_of(" \t"));
                                                path.erase(path.find_last_not_of(" \t") + 1);
                                                if (!path.empty()) {
                                                    search_paths.push_back(path);
                                                }
                                                paths_str.erase(0, pos + delimiter.length());
                                            }
                                            
                                            // Handle last path
                                            paths_str.erase(0, paths_str.find_first_not_of(" \t"));
                                            paths_str.erase(paths_str.find_last_not_of(" \t") + 1);
                                            if (!paths_str.empty()) {
                                                search_paths.push_back(paths_str);
                                            }
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    } catch (...) {
                        // If we can't read the .var file, fall back to default paths
                        search_paths = {
                            "/home/sblo/MyApps",
                            "/home/sblo/upp/uppsrc"
                        };
                    }
                    
                    // If no paths were found, use defaults
                    if (search_paths.empty()) {
                        search_paths = {
                            "/home/sblo/MyApps",
                            "/home/sblo/upp/uppsrc"
                        };
                    }
                    
                    // Get all packages and filter by verbosity
                    auto all_packages = workspace->get_all_packages();
                    
                    if(!verbose && !search_paths.empty()) {
                        // Filter to only packages that come from the first directory
                        std::vector<std::shared_ptr<UppPackage>> filtered_packages;
                        for(const auto& pkg : all_packages) {
                            // Check if package path starts with the first search path
                            if(pkg->path.substr(0, search_paths[0].length()) == search_paths[0] &&
                               pkg->path.length() > search_paths[0].length() && 
                               pkg->path[search_paths[0].length()] == '/') {
                                filtered_packages.push_back(pkg);
                            }
                        }
                        
                        if(filtered_packages.empty()) {
                            std::cout << "  No packages found in first directory (" << search_paths[0] << ")\n";
                        } else {
                            for(const auto& pkg : filtered_packages) {
                                std::string primary_marker = pkg->is_primary ? " (primary)" : "";
                                std::cout << "- " << pkg->name << primary_marker << "\n";
                            }
                        }
                    } else {
                        // Show all packages
                        if(all_packages.empty()) {
                            std::cout << "  No packages found\n";
                        } else {
                            for(const auto& pkg : all_packages) {
                                std::string primary_marker = pkg->is_primary ? " (primary)" : "";
                                std::cout << "- " << pkg->name << primary_marker << "\n";
                            }
                        }
                    }
                } else {
                    std::cout << "  Current assembly has no workspace\n";
                }
            } else {
                std::cout << "  No active assembly. Use 'upp.startup.open <name>' to set an active assembly\n";
            }

        } else if(cmd == "upp.gui"){
            // Launch the U++ assembly GUI
            auto assembly = std::make_shared<UppAssembly>();
            // For now, try to find an existing assembly in VFS
            if(auto upp_root = vfs.resolve("/upp")) {
                // For simplicity, we'll just initialize GUI with a basic assembly
                UppAssemblyGui gui(assembly);
                gui.init();
                std::cout << "U++ Assembly GUI launched (press any key to exit GUI mode and return to shell)\n";
                // Note: In a full implementation, we would run the GUI here
                // For now, we'll just show a message and return
            } else {
                // Create an empty workspace
                auto workspace = std::make_shared<UppWorkspace>("default", "");
                assembly->set_workspace(workspace);
                UppAssemblyGui gui(assembly);
                gui.init();
                std::cout << "U++ Assembly GUI launched with empty workspace\n";
            }
            std::cout << "GUI mode would run here\n";

        } else if(cmd == "upp.asm.scan"){
            if(inv.args.empty()) throw std::runtime_error("upp.asm.scan <directory-path>");
            std::string scan_path = normalize_path(cwd.path, inv.args[0]);
            
            // Create a new assembly and scan for U++ packages
            auto assembly = std::make_shared<UppAssembly>();
            if(assembly->detect_packages_from_directory(vfs, scan_path, false)) {
                std::cout << "Scanned " << scan_path << " for U++ packages\n";
                
                // Count the packages found
                int package_count = 0;
                if(auto ws = assembly->get_workspace()) {
                    auto packages = ws->get_all_packages();
                    package_count = packages.size();
                    std::cout << "Found " << package_count << " U++ package(s):\n";
                    
                    for(const auto& pkg : packages) {
                        std::cout << "  - " << pkg->name << " (" << pkg->files.size() << " files, " << pkg->dependencies.size() << " deps)\n";
                    }
                }
                
                // Create VFS structure for the detected packages
                assembly->create_vfs_structure(vfs, cwd.primary_overlay);
            } else {
                throw std::runtime_error("Failed to scan directory: " + scan_path);
            }

        } else if(cmd == "upp.asm.load.host"){
            std::cout << "DEBUG: upp.asm.load.host command called with path: " << (inv.args.empty() ? "(empty)" : inv.args[0]) << std::endl;
            std::cout.flush();
            if(inv.args.empty()) throw std::runtime_error("upp.asm.load.host <host-var-file-path>");
            
            std::string host_path = inv.args[0];  // Use as-is, not normalized (host path)
            std::cout << "Processing host path: " << host_path << std::endl;
            std::cout.flush();
            
            // Extract directory and filename
            size_t last_slash = host_path.find_last_of("/\\");
            std::string host_dir, var_filename;
            if (last_slash != std::string::npos) {
                host_dir = host_path.substr(0, last_slash);
                var_filename = host_path.substr(last_slash + 1);
            } else {
                host_dir = ".";  // Current directory if no path separators
                var_filename = host_path;
            }
            
            std::cout << "Host directory: " << host_dir << ", filename: " << var_filename << std::endl;
            std::cout.flush();
            
            // Check if the host file exists before attempting to mount
            std::ifstream test_file(host_path);
            if (!test_file.good()) {
                std::cout << "File does not exist or is not accessible" << std::endl;
                std::cout.flush();
                throw std::runtime_error("Host file does not exist or is not accessible: " + host_path);
            }
            test_file.close();
            
            std::cout << "Host file exists, proceeding with mount" << std::endl;
            std::cout.flush();
            
            // Mount the directory containing the .var file to the VFS
            std::string vfs_mount_point = "/mnt/host_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(getpid());
            
            std::cout << "Attempting to mount: " << host_dir << " -> " << vfs_mount_point << std::endl;
            std::cout.flush();
            
            try {
                vfs.mountFilesystem(host_dir, vfs_mount_point, cwd.primary_overlay);  // Use same overlay as shell
                std::cout << "Mount operation completed successfully" << std::endl;
                std::cout.flush();
            } catch (const std::exception& e) {
                std::cout << "Mount failed with exception: " << e.what() << std::endl;
                std::cout.flush();
                throw std::runtime_error("Failed to mount host directory: " + host_dir + " - Exception: " + e.what());
            } catch (...) {
                std::cout << "Mount failed with unknown exception" << std::endl;
                std::cout.flush();
                throw std::runtime_error("Failed to mount host directory: " + host_dir + " - Unknown exception");
            }
            
            std::cout << "Mounted host directory: " << host_dir << " -> " << vfs_mount_point << "\n";
            std::cout.flush();
            
            // Give the mount system time to initialize
            usleep(300000); // 300ms delay
            
            // Load the .var file from the mounted location
            std::string var_vfs_path = vfs_mount_point + "/" + var_filename;
            
            std::cout << "Attempting to read file at: " << var_vfs_path << std::endl;
            std::cout.flush();
            
            // Debug: Try to list the mount point to see what's there
            try {
                auto overlay_ids = vfs.overlaysForPath(vfs_mount_point);
                auto dir_listing = vfs.listDir(vfs_mount_point, overlay_ids);
                std::cout << "Directory listing at mount point (" << dir_listing.size() << " entries):\n";
                std::cout.flush();
                for (const auto& [entry_name, entry] : dir_listing) {
                    std::cout << "  - " << entry_name;
                    if (entry.types.count('d') > 0) std::cout << " (dir)";
                    if (entry.types.count('f') > 0) std::cout << " (file)";
                    std::cout << "\n";
                    std::cout.flush();
                }
            } catch (const std::exception& e) {
                std::cout << "Could not list mount point directory - Exception: " << e.what() << "\n";
                std::cout.flush();
            } catch (...) {
                std::cout << "Could not list mount point directory - Unknown exception\n";
                std::cout.flush();
            }
            
            // Read the .var file content from VFS
            std::string var_content;
            try {
                var_content = vfs.read(var_vfs_path, std::nullopt);
                std::cout << "Successfully read .var file content (" << var_content.length() << " bytes)\n";
                std::cout.flush();
            } catch (const std::exception& e) {
                std::cout << "Failed to read .var file - Exception: " << e.what() << "\n";
                std::cout.flush();
                throw std::runtime_error("Failed to read .var file from mounted location. Expected at: " + var_vfs_path + " (from host: " + host_path + ") - Exception: " + e.what());
            } catch (...) {
                std::cout << "Failed to read .var file - Unknown exception\n";
                std::cout.flush();
                throw std::runtime_error("Failed to read .var file from mounted location. Expected at: " + var_vfs_path + " (from host: " + host_path + ") - Unknown exception");
            }
            
            // Create UppAssembly instance and load the .var file
            auto assembly = std::make_shared<UppAssembly>();
            if(assembly->load_from_content(var_content, var_vfs_path)) {
                std::cout << "Loaded U++ assembly from host: " << host_path << "\n";
                
                // Add to VFS structure
                assembly->create_vfs_structure(vfs, cwd.primary_overlay);
                
                // Also process the directory for additional U++ packages
                assembly->detect_packages_from_directory(vfs, vfs_mount_point, false);
            } else {
                throw std::runtime_error("Failed to parse U++ assembly: " + host_path);
            }

        } else if(cmd == "upp.asm.refresh"){
            // Refresh all packages in active assembly
            // Parse flags first
            bool verbose = false;
            
            for(const auto& arg : inv.args) {
                if(arg == "-v") {
                    verbose = true;
                }
            }
            
            if(verbose) {
                std::cout << "Refreshing U++ assembly packages in active assembly..." << std::endl;
            }
            
            if(!g_current_assembly) {
                throw std::runtime_error("No active assembly. Use 'upp.startup.open <name>' to set an active assembly");
            }
            
            auto workspace = g_current_assembly->get_workspace();
            if(!workspace) {
                throw std::runtime_error("Active assembly has no workspace");
            }

            // Preserve primary package selection and clear existing packages before refresh
            std::string original_primary = workspace->primary_package;
            workspace->packages.clear();
            workspace->primary_package.clear();
            
            // Get the UPP paths from the assembly to find packages
            std::vector<std::string> upp_paths;
            
            // First, get the workspace path to look for .var file content
            if (!workspace->assembly_path.empty()) {
                try {
                    std::string var_content = vfs.read(workspace->assembly_path, std::nullopt);
                    std::istringstream stream(var_content);
                    std::string line;
                    
                    while (std::getline(stream, line)) {
                        // Look for UPP= line (handle both "UPP=" and "UPP =" formats)
                        if (line.length() >= 3 && line.substr(0, 3) == "UPP") {
                            // Find the equals sign
                            size_t eq_pos = line.find('=');
                            if (eq_pos != std::string::npos) {
                                // Extract the paths from UPP = "path1;path2;..." or UPP="path1;path2;..."
                                size_t start_quote = line.find('"', eq_pos);
                                size_t end_quote = line.rfind('"');
                                if (start_quote != std::string::npos && end_quote != std::string::npos && end_quote > start_quote) {
                                    std::string paths_str = line.substr(start_quote + 1, end_quote - start_quote - 1);
                                    
                                    // Split paths by semicolon
                                    size_t pos = 0;
                                    std::string delimiter = ";";
                                    while ((pos = paths_str.find(delimiter)) != std::string::npos) {
                                        std::string path = paths_str.substr(0, pos);
                                        // Remove leading/trailing whitespace
                                        path.erase(0, path.find_first_not_of(" \t"));
                                        path.erase(path.find_last_not_of(" \t") + 1);
                                        if (!path.empty()) {
                                            upp_paths.push_back(path);
                                        }
                                        paths_str.erase(0, pos + delimiter.length());
                                    }
                                    
                                    // Handle last path
                                    paths_str.erase(0, paths_str.find_first_not_of(" \t"));
                                    paths_str.erase(paths_str.find_last_not_of(" \t") + 1);
                                    if (!paths_str.empty()) {
                                        upp_paths.push_back(paths_str);
                                    }
                                }
                                break;
                            }
                        }
                    }
                } catch (...) {
                    // If we can't read the .var file, use default paths
                    if (verbose) {
                        std::cout << "Could not read .var file, using default paths..." << std::endl;
                    }
                    upp_paths = {
                        "/home/sblo/MyApps",
                        "/home/sblo/upp/uppsrc"
                    };
                }
            }
            
            // If no UPP paths were found in the .var file, use defaults
            if (upp_paths.empty()) {
                if (verbose) {
                    std::cout << "No UPP paths found in .var file, using default paths..." << std::endl;
                }
                upp_paths = {
                    "/home/sblo/MyApps",
                    "/home/sblo/upp/uppsrc"
                };
            }
            
            // Process each UPP path to find and refresh packages
            // Add a static counter to make mount points unique across multiple paths
            static int mount_counter = 0;
            for(const auto& upp_path : upp_paths) {
                if (verbose) {
                    std::cout << "Scanning UPP path: " << upp_path << std::endl;
                }
                
                // Use a temporary assembly to detect packages without mutating the active workspace
                UppAssembly path_assembly;
                path_assembly.detect_packages_from_directory(vfs, upp_path, verbose);
                if (auto path_workspace = path_assembly.get_workspace()) {
                    for (auto& pkg : path_workspace->get_all_packages()) {
                        if (!workspace->get_package(pkg->name)) {
                            pkg->is_primary = false;
                            workspace->add_package(pkg);
                        }
                    }
                }
                
                // Now perform our own recursive directory scan to make sure all subdirectories are visited
                // First, try to mount the directory to make it accessible in VFS
                std::string vfs_mount_point = "/mnt/host_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(getpid()) + "_" + std::to_string(++mount_counter);
                
                try {
                    // Mount the host directory to allow VFS access
                    vfs.mountFilesystem(upp_path, vfs_mount_point, cwd.primary_overlay);
                    
                    // Give the mount system time to initialize
                    usleep(100000); // 100ms delay
                    
                    // Now scan the mounted path
                    try {
                        // Get all overlays for this mounted path
                        auto overlay_ids = vfs.overlaysForPath(vfs_mount_point);
                        auto dir_listing = vfs.listDir(vfs_mount_point, overlay_ids);
                        
                        // Process subdirectories recursively
                        std::vector<std::string> subdirs_to_scan;
                        for(const auto& [entry_name, entry] : dir_listing) {
                            // Only process directories
                            if(entry.types.count('d') > 0) {  // 'd' indicates directory
                                std::string subdir_path = vfs_mount_point + "/" + entry_name;
                                if (verbose) {
                                    std::string original_path = upp_path + "/" + entry_name;
                                    std::cout << "Visiting directory: " << original_path << std::endl;
                                }
                                subdirs_to_scan.push_back(subdir_path);
                            }
                        }
                        
                        // Process subdirectories recursively using a stack
                        while(!subdirs_to_scan.empty()) {
                            std::string current_dir = subdirs_to_scan.back();
                            subdirs_to_scan.pop_back();
                            
                            // Convert back to original path for user-friendly output
                            std::string orig_path = current_dir;
                            if (current_dir.substr(0, vfs_mount_point.length()) == vfs_mount_point) {
                                orig_path = upp_path + current_dir.substr(vfs_mount_point.length());
                            }
                            
                            if (verbose) {
                                std::cout << "Visiting directory: " << orig_path << std::endl;
                            }
                            
                            // Check if this directory contains a .upp file with the same name
                            size_t last_slash = orig_path.find_last_of('/');
                            std::string dir_name = (last_slash != std::string::npos) ? 
                                                  orig_path.substr(last_slash + 1) : orig_path;
                            std::string upp_file_path = orig_path + "/" + dir_name + ".upp";
                            
                            if (verbose) {
                                std::cout << "Checking for U++ package file: " << upp_file_path << std::endl;
                            }
                            
                            try {
                                // Try to read the original path in VFS (after mount)
                                std::string mapped_vfs_path = current_dir + "/" + dir_name + ".upp";
                                std::string upp_content = vfs.read(mapped_vfs_path, std::nullopt);
                                
                                if (verbose) {
                                    std::cout << "Found U++ package: " << upp_file_path << std::endl;
                                }
                                
                                // Add this package to the assembly if found and not already present
                                if (auto ws = g_current_assembly->get_workspace()) {
                                    if (!ws->get_package(dir_name)) {
                                        auto pkg = std::make_shared<UppPackage>(dir_name, orig_path);
                                        g_current_assembly->parse_upp_file_content(upp_content, *pkg);
                                        pkg->is_primary = false;
                                        ws->add_package(pkg);
                                    }
                                }
                            } catch (...) {
                                // If there's no .upp file, continue to next
                                if (verbose) {
                                    std::cout << "No U++ package file found in directory: " << orig_path << std::endl;
                                }
                            }
                            
                            // Add subdirectories of current directory to scan list
                            try {
                                auto subdir_overlay_ids = vfs.overlaysForPath(current_dir);
                                auto subdir_listing = vfs.listDir(current_dir, subdir_overlay_ids);
                                for(const auto& [subentry_name, subentry] : subdir_listing) {
                                    // Only process directories
                                    if(subentry.types.count('d') > 0) {  // 'd' indicates directory
                                        std::string subsubdir_path = current_dir + "/" + subentry_name;
                                        std::string original_sub_path = upp_path + subsubdir_path.substr(vfs_mount_point.length());
                                        if (verbose) {
                                            std::cout << "Visiting directory: " << original_sub_path << std::endl;
                                        }
                                        subdirs_to_scan.push_back(subsubdir_path);
                                    }
                                }
                            } catch (...) {
                                if (verbose) {
                                    std::string orig_current = current_dir;
                                    if (current_dir.substr(0, vfs_mount_point.length()) == vfs_mount_point) {
                                        orig_current = upp_path + current_dir.substr(vfs_mount_point.length());
                                    }
                                    std::cout << "Cannot access subdirectory: " << orig_current << std::endl;
                                }
                            }
                        }
                    } catch (...) {
                        if (verbose) {
                            std::cout << "Could not list mounted directory: " << upp_path << std::endl;
                        }
                    }
                } catch (const std::exception& e) {
                    if (verbose) {
                        std::cout << "Could not mount directory to scan subdirectories: " << upp_path << " (error: " << e.what() << "), will use detect_packages_from_directory only" << std::endl;
                    }
                    // If we can't mount, just continue to the next path
                } catch (...) {
                    if (verbose) {
                        std::cout << "Could not mount directory to scan subdirectories: " << upp_path << ", will use detect_packages_from_directory only" << std::endl;
                    }
                    // If we can't mount, just continue to the next path
                }
            }

            // Restore primary package selection after refreshing packages
            bool primary_restored = false;
            for (auto& entry : workspace->packages) {
                entry.second->is_primary = false;
            }
            if (!original_primary.empty()) {
                if (auto primary_pkg = workspace->get_package(original_primary)) {
                    primary_pkg->is_primary = true;
                    workspace->primary_package = original_primary;
                    primary_restored = true;
                }
            }
            if (!primary_restored) {
                if (!workspace->packages.empty()) {
                    auto first_it = workspace->packages.begin();
                    first_it->second->is_primary = true;
                    workspace->primary_package = first_it->first;
                } else {
                    workspace->primary_package.clear();
                }
            }
            
            if (verbose) {
                // Count total packages found
                auto all_packages = workspace->get_all_packages();
                std::cout << "Finished refreshing. Total packages found: " << all_packages.size() << std::endl;
            }

        } else if(cmd == "upp.startup.load"){
            // Parse flags first
            bool use_host_path = false;  // default behavior: treat as VFS path without -H flag
            std::string path_arg;
            
            for(const auto& arg : inv.args) {
                if(arg == "-H") {
                    use_host_path = true;  // with -H flag: treat as host OS path
                } else if(arg != "-H") {
                    path_arg = arg;  // store the actual path argument
                }
            }
            
            if(path_arg.empty()) throw std::runtime_error("upp.startup.load <directory-path> [-H] (use -H for host OS path)");
            
            // If -H flag is provided, use host OS path behavior
            if(use_host_path) {
                std::string host_dir_path = path_arg;
                
                // Check if the host directory exists
                std::filesystem::path host_path{host_dir_path};
                if (!std::filesystem::exists(host_path)) {
                    throw std::runtime_error("Host directory does not exist: " + host_dir_path);
                }
                
                if (!std::filesystem::is_directory(host_path)) {
                    throw std::runtime_error("Path is not a directory: " + host_dir_path);
                }
                
                // Mount the directory to the VFS
                std::string vfs_mount_point = "/mnt/host_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(getpid());
                
                try {
                    vfs.mountFilesystem(host_dir_path, vfs_mount_point, cwd.primary_overlay);  // Use same overlay as shell
                } catch (const std::exception& e) {
                    throw std::runtime_error("Failed to mount host directory: " + host_dir_path + " - Exception: " + e.what());
                } catch (...) {
                    throw std::runtime_error("Failed to mount host directory: " + host_dir_path + " - Unknown exception");
                }
                
                // Give the mount system time to initialize
                usleep(300000); // 300ms delay
                
                // Use the mounted path to scan for .var files
                std::string mounted_dir_path = vfs_mount_point;
                
                // Scan directory for .var files
                std::vector<std::string> var_files;
                try {
                    auto overlay_ids = vfs.overlaysForPath(mounted_dir_path);
                    auto listing = vfs.listDir(mounted_dir_path, overlay_ids);
                    
                    for(const auto& [entry_name, entry] : listing){
                        // Check if it's a .var file
                        if(entry_name.length() > 4 && entry_name.substr(entry_name.length() - 4) == ".var"){
                            var_files.push_back(join_path(mounted_dir_path, entry_name));
                        }
                    }
                } catch(const std::exception& e) {
                    throw std::runtime_error("Failed to list directory " + mounted_dir_path + ": " + e.what());
                }
                
                if(var_files.empty()){
                    std::cout << "No .var files found in " << mounted_dir_path << " (mounted from host: " << host_dir_path << ")\n";
                    return result;
                }
                
                // Load each .var file
                for(const auto& var_file : var_files){
                    try {
                        std::string var_content = vfs.read(var_file, std::nullopt);
                        auto assembly = std::make_shared<UppAssembly>();
                        if(assembly->load_from_content(var_content, var_file)) {
                            g_startup_assemblies.push_back(assembly);
                            std::cout << "Loaded startup assembly: " << var_file << " (from host: " << host_dir_path << ")\n";
                            
                            // Add to VFS structure
                            assembly->create_vfs_structure(vfs, cwd.primary_overlay);
                        } else {
                            std::cout << "Failed to load startup assembly: " << var_file << "\n";
                        }
                    } catch(const std::exception& e) {
                        std::cout << "Error loading " << var_file << ": " << e.what() << "\n";
                    }
                }
            } else {
                // Without -H flag: treat as VFS path, load directly
                std::string dir_path = normalize_path(cwd.path, path_arg);
                
                // Check if directory exists
                auto dir_hits = vfs.resolveMulti(dir_path);
                if(dir_hits.empty()) throw std::runtime_error("Directory not found: " + dir_path);
                
                bool is_dir = false;
                for(const auto& hit : dir_hits){
                    if(hit.node->isDir()){
                        is_dir = true;
                        break;
                    }
                }
                if(!is_dir) throw std::runtime_error("Not a directory: " + dir_path);
                
                // Scan directory for .var files
                std::vector<std::string> var_files;
                try {
                    auto overlay_ids = vfs.overlaysForPath(dir_path);
                    auto listing = vfs.listDir(dir_path, overlay_ids);
                    
                    for(const auto& [entry_name, entry] : listing){
                        // Check if it's a .var file
                        if(entry_name.length() > 4 && entry_name.substr(entry_name.length() - 4) == ".var"){
                            var_files.push_back(join_path(dir_path, entry_name));
                        }
                    }
                } catch(const std::exception& e) {
                    throw std::runtime_error("Failed to list directory " + dir_path + ": " + e.what());
                }
                
                if(var_files.empty()){
                    std::cout << "No .var files found in " << dir_path << "\n";
                    return result;
                }
                
                // Load each .var file
                for(const auto& var_file : var_files){
                    try {
                        std::string var_content = vfs.read(var_file, std::nullopt);
                        auto assembly = std::make_shared<UppAssembly>();
                        if(assembly->load_from_content(var_content, var_file)) {
                            g_startup_assemblies.push_back(assembly);
                            std::cout << "Loaded startup assembly: " << var_file << "\n";
                            
                            // Add to VFS structure
                            assembly->create_vfs_structure(vfs, cwd.primary_overlay);
                        } else {
                            std::cout << "Failed to load startup assembly: " << var_file << "\n";
                        }
                    } catch(const std::exception& e) {
                        std::cout << "Error loading " << var_file << ": " << e.what() << "\n";
                    }
                }
            }

        } else if(cmd == "upp.startup.list"){
            if(g_startup_assemblies.empty()){
                std::cout << "No startup assemblies loaded\n";
            } else {
                std::cout << "Startup assemblies (" << g_startup_assemblies.size() << "):\n";
                for(size_t i = 0; i < g_startup_assemblies.size(); ++i){
                    auto assembly = g_startup_assemblies[i];
                    if(auto workspace = assembly->get_workspace()){
                        std::cout << "  " << i << ": " << workspace->name << " (" << workspace->base_dir << ")\n";
                        auto packages = workspace->get_all_packages();
                        for(const auto& pkg : packages){
                            std::string primary_marker = pkg->is_primary ? " (primary)" : "";
                            std::cout << "    - " << pkg->name << primary_marker << "\n";
                        }
                    }
                }
            }

        } else if(cmd == "upp.startup.open") {
            // Parse flags first
            bool verbose = false;
            std::string assembly_name;
            
            for(const auto& arg : inv.args) {
                if(arg == "-v") {
                    verbose = true;
                } else if(arg != "-v") {
                    if(assembly_name.empty()) {
                        assembly_name = arg;  // store the actual assembly name
                    } else {
                        throw std::runtime_error("upp.startup.open <name> [-v] (too many arguments)");
                    }
                }
            }
            
            // If verbose flag is set but no assembly name provided, search default locations
            if(assembly_name.empty() && verbose) {
                // Default search paths - these are common locations for .var files
                std::vector<std::string> search_paths = {
                    "/home/sblo/.config/u++/theide",
                    "/home/sblo/upp",
                    "/home/sblo/MyApps"
                };
                
                std::cout << "Verbose mode: No assembly name provided, searching in default locations...\n";
                
                bool found = false;
                for(const auto& search_path : search_paths) {
                    if(verbose) {
                        std::cout << "Scanning directory: " << search_path << " for .var files\n";
                    }
                
                    try {
                        auto overlay_ids = vfs.overlaysForPath(search_path);
                        auto dir_listing = vfs.listDir(search_path, overlay_ids);
                        
                        for(const auto& [entry_name, entry] : dir_listing) {
                            // Check if it's a .var file
                            if(entry_name.length() > 4 && entry_name.substr(entry_name.length() - 4) == ".var") {
                                std::string var_file_path = join_path(search_path, entry_name);
                                
                                if(verbose) {
                                    std::cout << "Found .var file: " << var_file_path << "\n";
                                }
                                
                                try {
                                    std::string var_content = vfs.read(var_file_path, std::nullopt);
                                    auto assembly = std::make_shared<UppAssembly>();
                                    if(assembly->load_from_content(var_content, var_file_path)) {
                                        g_startup_assemblies.push_back(assembly);
                                        
                                        if(verbose) {
                                            std::cout << "Loaded startup assembly: " << var_file_path << "\n";
                                            
                                            // Also try to detect packages from the directory
                                            std::string base_dir = var_file_path.substr(0, var_file_path.find_last_of('/'));
                                            assembly->detect_packages_from_directory(vfs, base_dir, verbose);
                                        } else {
                                            std::cout << "Loaded startup assembly: " << var_file_path << "\n";
                                        }
                                        
                                        // Add to VFS structure
                                        assembly->create_vfs_structure(vfs, cwd.primary_overlay);
                                        
                                        found = true;
                                    } else {
                                        if(verbose) {
                                            std::cout << "Failed to load startup assembly: " << var_file_path << "\n";
                                        }
                                    }
                                } catch(const std::exception& e) {
                                    if(verbose) {
                                        std::cout << "Error loading " << var_file_path << ": " << e.what() << "\n";
                                    }
                                }
                            }
                        }
                    } catch(const std::exception& e) {
                        if(verbose) {
                            std::cout << "Error scanning directory " << search_path << ": " << e.what() << "\n";
                        }
                        continue; // Continue to next search path
                    }
                }
                
                if(found) {
                    std::cout << "Scanning completed, loaded new assemblies. Use upp.startup.list to see available assemblies.\n";
                } else {
                    std::cout << "No .var files found in default locations.\n";
                }
            }
            // If assembly name is provided, search in loaded assemblies
            else if(!assembly_name.empty()) {
                bool found = false;
                
                if(verbose) {
                    std::cout << "Searching for startup assembly: " << assembly_name << " in " << g_startup_assemblies.size() << " loaded assemblies\n";
                }
                
                // Look for the assembly by name
                for(size_t i = 0; i < g_startup_assemblies.size(); ++i) {
                    const auto& assembly = g_startup_assemblies[i];
                    if(auto workspace = assembly->get_workspace()) {
                        if(verbose) {
                            std::cout << "Checking assembly " << i << ": " << workspace->name << "\n";
                        }
                        if(workspace->name == assembly_name) {
                            // Load the .var file content using the assembly_path
                            std::string var_file_path = workspace->assembly_path;
                            if(verbose) {
                                std::cout << "Found assembly, loading from: " << var_file_path << "\n";
                            }
                            try {
                                std::string var_content = vfs.read(var_file_path, std::nullopt);
                                if(assembly->load_from_content(var_content, var_file_path)) {
                                    std::cout << "Opened startup assembly: " << assembly_name << " (" << var_file_path << ")\n";
                                    
                                    // Add to VFS structure
                                    assembly->create_vfs_structure(vfs, cwd.primary_overlay);
                                    
                                    // Set this as the current assembly
                                    g_current_assembly = assembly;
                                    
                                    found = true;
                                    break;
                                } else {
                                    throw std::runtime_error("Failed to load startup assembly: " + var_file_path);
                                }
                            } catch(const std::exception& e) {
                                throw std::runtime_error("Error opening " + var_file_path + ": " + e.what());
                            }
                        }
                    }
                }
                
                if(!found) {
                    throw std::runtime_error("Startup assembly not found: " + assembly_name);
                }
            }
            // If no arguments provided at all
            else {
                throw std::runtime_error("upp.startup.open <name> [-v] (open a named startup assembly, -v for verbose or to scan for .var files in default locations)");
            }

        } else if(cmd == "upp.wksp.open") {
            if(inv.args.empty()) throw std::runtime_error("upp.wksp.open <pkg-name> | upp.wksp.open -p <path> (open a package from list or path)");
            
            // Check if the first argument is the -p flag for path-based opening
            if(inv.args[0] == "-p") {
                // Parse flags first
                bool verbose = false;
                std::string package_path;
                
                for(size_t i = 1; i < inv.args.size(); ++i) {  // Start from index 1 to skip "-p"
                    if(inv.args[i] == "-v") {
                        verbose = true;
                    } else if(inv.args[i] != "-v") {
                        if(package_path.empty()) {
                            package_path = inv.args[i];  // store the actual path argument
                        } else {
                            throw std::runtime_error("upp.wksp.open -p <path> [-v] (too many arguments)");
                        }
                    }
                }
                
                if(package_path.empty()) throw std::runtime_error("upp.wksp.open -p <path> [-v] (open a U++ package as workspace from path, -v for verbose)");
                
                // Create a new assembly and workspace
                auto new_assembly = std::make_shared<UppAssembly>();
                
                // Try to detect packages from the specified directory
                if(new_assembly->detect_packages_from_directory(vfs, package_path, verbose)) {
                    // Set this as the current assembly
                    g_current_assembly = new_assembly;
                    
                    // Get the workspace to show which packages were detected
                    auto workspace = g_current_assembly->get_workspace();
                    if(workspace) {
                        std::cout << "Opened workspace: " << workspace->name << "\n";
                        auto all_packages = workspace->get_all_packages();
                        if(!all_packages.empty()) {
                            std::cout << "Found " << all_packages.size() << " package(s):\n";
                            for(const auto& pkg : all_packages) {
                                std::string primary_marker = pkg->is_primary ? " (primary)" : "";
                                std::cout << "  - " << pkg->name << primary_marker << "\n";
                            }
                        } else {
                            std::cout << "No packages found in directory: " << package_path << "\n";
                        }
                    } else {
                        std::cout << "Opened workspace from: " << package_path << "\n";
                    }
                } else {
                    throw std::runtime_error("Failed to open workspace from: " + package_path);
                }
            } else {
                // Primary use case: open by package name from the list
                std::string package_name = inv.args[0];
                
                // Check if we already have an active assembly
                if(g_current_assembly) {
                    // Get the workspace from the current assembly
                    auto workspace = g_current_assembly->get_workspace();
                    if(workspace) {
                        // Check if the package exists in the current workspace
                        auto pkg = workspace->get_package(package_name);
                        if(pkg) {
                            // Mark this package as primary in the existing workspace
                            // Reset primary packages
                            for(const auto& existing_pkg : workspace->get_all_packages()) {
                                existing_pkg->is_primary = false;
                            }
                            // Set the requested package as primary
                            pkg->is_primary = true;
                            workspace->primary_package = package_name;
                            std::cout << "Opened workspace with package: " << package_name << " (as primary)\n";
                        } else {
                            // Package not in current workspace, try to find it in VFS
                            // Similar to what upp.asm.list does when no packages are found
                            
                            // First, let's try to detect packages from standard U++ paths
                            std::vector<std::string> search_paths = {
                                "/home/sblo/MyApps",
                                "/home/sblo/upp/uppsrc"
                            };
                            
                            bool found_package = false;
                            for(const auto& search_path : search_paths) {
                                try {
                                    // Try to detect packages from each search path
                                    if(g_current_assembly->detect_packages_from_directory(vfs, search_path, false)) {
                                        auto workspace = g_current_assembly->get_workspace();
                                        if(workspace) {
                                            auto pkg = workspace->get_package(package_name);
                                            if(pkg) {
                                                // Mark this package as primary in the existing workspace
                                                // Reset primary packages
                                                for(const auto& existing_pkg : workspace->get_all_packages()) {
                                                    existing_pkg->is_primary = false;
                                                }
                                                // Set the requested package as primary
                                                pkg->is_primary = true;
                                                workspace->primary_package = package_name;
                                                std::cout << "Opened workspace with package: " << package_name << " (as primary)\n";
                                                found_package = true;
                                                break;
                                            }
                                        }
                                    }
                                } catch (...) {
                                    // If one path fails, continue to the next
                                    continue;
                                }
                            }
                            
                            if(!found_package) {
                                // Package still not found, try to load directly from VFS
                                // Check if package exists at the standard location in VFS
                                std::string package_dir_path = "/home/sblo/MyApps/" + package_name;
                                std::string upp_file_path = package_dir_path + "/" + package_name + ".upp";
                                
                                try {
                                    std::string upp_content = vfs.read(upp_file_path, std::nullopt);
                                    
                                    // If we successfully read the .upp file, this is a valid U++ package
                                    auto pkg = std::make_shared<UppPackage>(package_name, package_dir_path, true); // Mark as primary
                                    
                                    // Parse the .upp file to get more information
                                    g_current_assembly->parse_upp_file_content(upp_content, *pkg);
                                    
                                    workspace->add_package(pkg);
                                    
                                    // Set the requested package as primary
                                    pkg->is_primary = true;
                                    workspace->primary_package = package_name;
                                    std::cout << "Opened workspace with package: " << package_name << "\n";
                                    found_package = true;
                                } catch (...) {
                                    // Try other standard locations
                                    package_dir_path = "/home/sblo/upp/uppsrc/" + package_name;
                                    upp_file_path = package_dir_path + "/" + package_name + ".upp";
                                    
                                    try {
                                        std::string upp_content = vfs.read(upp_file_path, std::nullopt);
                                        
                                        // If we successfully read the .upp file, this is a valid U++ package
                                        auto pkg = std::make_shared<UppPackage>(package_name, package_dir_path, true); // Mark as primary
                                        
                                        // Parse the .upp file to get more information
                                        g_current_assembly->parse_upp_file_content(upp_content, *pkg);
                                        
                                        workspace->add_package(pkg);
                                        
                                        // Set the requested package as primary
                                        pkg->is_primary = true;
                                        workspace->primary_package = package_name;
                                        std::cout << "Opened workspace with package: " << package_name << "\n";
                                        found_package = true;
                                    } catch (...) {
                                        // If not found in standard locations, check in mounted paths
                                        // Check if the package exists in any currently mounted locations
                                        // This is a simplified implementation - we'll try to find the package in VFS
                                        
                                        // If we still can't find the package, throw an error
                                        throw std::runtime_error("Package not found: " + package_name + 
                                            ". Use 'upp.asm.list' to see available packages or 'upp.wksp.open -p <path>' to open from path.");
                                    }
                                }
                            }
                        }
                    } else {
                        // No workspace exists in the current assembly
                        // Create a new workspace and try to load the package
                        workspace = std::make_shared<UppWorkspace>("default", "");
                        g_current_assembly->set_workspace(workspace);
                        
                        // Try to find the package in standard locations as above
                        std::vector<std::string> search_paths = {
                            "/home/sblo/MyApps",
                            "/home/sblo/upp/uppsrc"
                        };
                        
                        bool found_package = false;
                        for(const auto& search_path : search_paths) {
                            std::string package_dir_path = search_path + "/" + package_name;
                            std::string upp_file_path = package_dir_path + "/" + package_name + ".upp";
                            
                            try {
                                std::string upp_content = vfs.read(upp_file_path, std::nullopt);
                                
                                // If we successfully read the .upp file, this is a valid U++ package
                                auto pkg = std::make_shared<UppPackage>(package_name, package_dir_path, true); // Mark as primary
                                
                                // Parse the .upp file to get more information
                                g_current_assembly->parse_upp_file_content(upp_content, *pkg);
                                
                                workspace->add_package(pkg);
                                
                                // Set the requested package as primary
                                pkg->is_primary = true;
                                workspace->primary_package = package_name;
                                std::cout << "Opened workspace with package: " << package_name << "\n";
                                found_package = true;
                                break;
                            } catch (...) {
                                // If there's no .upp file at this location, continue to next path
                                continue;
                            }
                        }
                        
                        if(!found_package) {
                            throw std::runtime_error("Package not found: " + package_name + 
                                ". Use 'upp.asm.list' to see available packages or 'upp.wksp.open -p <path>' to open from path.");
                        }
                    }
                } else {
                    // No active assembly, create a new one and try to load the package
                    auto new_assembly = std::make_shared<UppAssembly>();
                    
                    // We need to locate the package based on the package name
                    // First, let's try to use the standard U++ package search paths
                    std::vector<std::string> search_paths = {
                        "/home/sblo/MyApps",
                        "/home/sblo/upp/uppsrc"
                    };
                    
                    bool package_found = false;
                    for(const auto& search_path : search_paths) {
                        std::string package_dir_path = search_path + "/" + package_name;
                        std::string upp_file_path = package_dir_path + "/" + package_name + ".upp";
                        
                        // Try to read the .upp file to confirm it's a U++ package
                        try {
                            std::string upp_content = vfs.read(upp_file_path, std::nullopt);
                            
                            // If we successfully read the .upp file, this is a valid U++ package
                            // Create a workspace with this package
                            auto workspace = std::make_shared<UppWorkspace>("default", package_dir_path);
                            
                            auto pkg = std::make_shared<UppPackage>(package_name, package_dir_path, true); // Mark as primary
                            
                            // Parse the .upp file to get more information
                            new_assembly->parse_upp_file_content(upp_content, *pkg);
                            
                            workspace->add_package(pkg);
                            new_assembly->set_workspace(workspace);
                            
                            // Set this as the current assembly
                            g_current_assembly = new_assembly;
                            std::cout << "Opened workspace with package: " << package_name << "\n";
                            package_found = true;
                            break;
                        } catch (...) {
                            // If there's no .upp file at this location, continue to next path
                            continue;
                        }
                    }
                    
                    if(!package_found) {
                        throw std::runtime_error("Package not found: " + package_name + 
                            ". Use 'upp.asm.list' to see available packages or 'upp.wksp.open -p <path>' to open from path.");
                    }
                }
            }

        } else if(cmd == "upp.wksp.close") {
            // Close the current workspace by setting the current assembly to nullptr
            g_current_assembly = nullptr;
            std::cout << "Closed current workspace\n";

        } else if(cmd == "upp.wksp.pkg.list") {
            // List packages in the current workspace
            std::cout << "U++ Workspace packages:\n";
            if(g_current_assembly) {
                auto workspace = g_current_assembly->get_workspace();
                if(workspace) {
                    auto all_packages = workspace->get_all_packages();
                    if(all_packages.empty()) {
                        std::cout << "  No packages found in current workspace\n";
                    } else {
                        for(const auto& pkg : all_packages) {
                            std::string primary_marker = pkg->is_primary ? " (primary)" : "";
                            std::cout << "- " << pkg->name << primary_marker << "\n";
                        }
                    }
                } else {
                    std::cout << "  Current workspace has no packages\n";
                }
            } else {
                std::cout << "  No active workspace. Use 'upp.wksp.open <path>' to open a workspace\n";
            }

        } else if(cmd == "upp.wksp.pkg.set") {
            if(inv.args.empty()) throw std::runtime_error("upp.wksp.pkg.set <package-name> (set active package in workspace)");
            
            std::string package_name = inv.args[0];
            
            if(g_current_assembly) {
                auto workspace = g_current_assembly->get_workspace();
                if(workspace) {
                    auto pkg = workspace->get_package(package_name);
                    if(pkg) {
                        // For now, we'll just mark it as the primary package for activation
                        // In a real implementation, we'd track the currently active package separately
                        // Reset primary packages
                        for(const auto& existing_pkg : workspace->get_all_packages()) {
                            existing_pkg->is_primary = false;
                        }
                        // Set the new package as primary
                        pkg->is_primary = true;
                        workspace->primary_package = package_name;
                        std::cout << "Set active package: " << package_name << "\n";
                    } else {
                        throw std::runtime_error("Package not found in workspace: " + package_name);
                    }
                } else {
                    throw std::runtime_error("Current workspace has no packages");
                }
            } else {
                throw std::runtime_error("No active workspace. Use 'upp.wksp.open <path>' first");
            }

        } else if(cmd == "upp.wksp.file.list") {
            // List files in the active package of the current workspace
            std::cout << "U++ Workspace active package files:\n";
            if(g_current_assembly) {
                auto workspace = g_current_assembly->get_workspace();
                if(workspace) {
                    // Find the primary (active) package
                    auto primary_pkg = workspace->get_primary_package();
                    if(primary_pkg) {
                        if(primary_pkg->files.empty()) {
                            std::cout << "  No files found in active package '" << primary_pkg->name << "'\n";
                        } else {
                            std::cout << "Files in package '" << primary_pkg->name << "':\n";
                            for(const auto& file : primary_pkg->files) {
                                std::cout << "- " << file << "\n";
                            }
                        }
                    } else {
                        std::cout << "  No active package selected. Use 'upp.wksp.pkg.set <package-name>' to select a package.\n";
                    }
                } else {
                    std::cout << "  Current workspace has no packages\n";
                }
            } else {
                std::cout << "  No active workspace. Use 'upp.wksp.open <name>' to open a workspace\n";
            }

        } else if(cmd == "upp.wksp.file.set") {
            if(inv.args.empty()) throw std::runtime_error("upp.wksp.file.set <file-path> (set active file in editor)");
            
            std::string file_path = inv.args[0];
            
            if(g_current_assembly) {
                auto workspace = g_current_assembly->get_workspace();
                if(workspace) {
                    // Find the primary (active) package
                    auto primary_pkg = workspace->get_primary_package();
                    if(primary_pkg) {
                        // Check if the file exists in the active package
                        bool file_found = false;
                        std::string matched_pkg_file;
                        for(const auto& pkg_file : primary_pkg->files) {
                            // Match by filename only (last part after slash)
                            size_t last_slash = pkg_file.find_last_of("/\\");
                            std::string filename = (last_slash != std::string::npos) ? 
                                pkg_file.substr(last_slash + 1) : pkg_file;
                            
                            size_t target_last_slash = file_path.find_last_of("/\\");
                            std::string target_filename = (target_last_slash != std::string::npos) ? 
                                file_path.substr(target_last_slash + 1) : file_path;
                            
                            if(filename == target_filename) {
                                file_found = true;
                                matched_pkg_file = pkg_file;
                                break;
                            }
                        }
                        
                        if(file_found) {
                            // Construct the full path by combining package path with matched file
                            std::string full_path;
                            if(matched_pkg_file.empty() || matched_pkg_file[0] != '/') {
                                // Relative path - combine with package path
                                full_path = primary_pkg->path + "/" + matched_pkg_file;
                            } else {
                                // Already absolute path
                                full_path = matched_pkg_file;
                            }

                            // Translate the host path to VFS-mounted path
                            auto vfs_mapped = vfs.mapFromHostPath(full_path);
                            if(vfs_mapped.has_value()) {
                                std::cout << "Set active file: " << vfs_mapped.value() << " (mapped from " << full_path << ")\n";
                                g_active_file_path = vfs_mapped.value();
                            } else {
                                // Fallback to original path if not mounted
                                std::cout << "Set active file: " << full_path << " (not mounted, using host path)\n";
                                g_active_file_path = full_path;
                            }
                        } else {
                            throw std::runtime_error("File not found in active package '" + 
                                primary_pkg->name + "': " + file_path);
                        }
                    } else {
                        throw std::runtime_error("No active package selected. Use 'upp.wksp.pkg.set <package-name>' first");
                    }
                } else {
                    throw std::runtime_error("Current workspace has no packages");
                }
            } else {
                throw std::runtime_error("No active workspace. Use 'upp.wksp.open <path>' first");
            }

        } else if(cmd == "upp.wksp.file.edit") {
            // Implementation of upp.wksp.file.edit [<file>]
            // If <file> is provided, set it as active file first (like upp.wksp.file.set)
            // Then open the file in the internal full-screen ncurses text editor
            
            std::string file_path;
            bool file_specified = !inv.args.empty();
            
            if(file_specified) {
                // Set the specified file as active first
                std::string target_file = inv.args[0];
                
                if(g_current_assembly) {
                    auto workspace = g_current_assembly->get_workspace();
                    if(workspace) {
                        // Find the primary (active) package
                        auto primary_pkg = workspace->get_primary_package();
                        if(primary_pkg) {
                            // Check if the file exists in the active package
                            bool file_found = false;
                            std::string matched_pkg_file;
                            for(const auto& pkg_file : primary_pkg->files) {
                                // Match by filename only (last part after slash)
                                size_t last_slash = pkg_file.find_last_of("/\\");
                                std::string filename = (last_slash != std::string::npos) ? 
                                    pkg_file.substr(last_slash + 1) : pkg_file;
                                
                                size_t target_last_slash = target_file.find_last_of("/\\");
                                std::string target_filename = (target_last_slash != std::string::npos) ? 
                                    target_file.substr(target_last_slash + 1) : target_file;
                                
                                if(filename == target_filename) {
                                    file_found = true;
                                    matched_pkg_file = pkg_file;
                                    break;
                                }
                            }
                            
                            if(file_found) {
                                std::cout << "Set active file: " << target_file << "\n";
                                file_path = matched_pkg_file; // Use the full package file path
                            } else {
                                throw std::runtime_error("File not found in active package '" + 
                                    primary_pkg->name + "': " + target_file);
                            }
                        } else {
                            throw std::runtime_error("No active package selected. Use 'upp.wksp.pkg.set <package-name>' first");
                        }
                    } else {
                        throw std::runtime_error("Current workspace has no packages");
                    }
                } else {
                    throw std::runtime_error("No active workspace. Use 'upp.wksp.open <path>' first");
                }
            } else {
                // No file specified - use the active file if one is set
                if (!g_active_file_path.empty()) {
                    file_path = g_active_file_path;
                } else {
                    throw std::runtime_error("No active file set. Use 'upp.wksp.file.set <file-path>' first, or specify a file path.");
                }
            }
            
            // Now open the file in the ncurses editor
            if(!file_path.empty()) {
                // Read existing content if file exists
                std::string content;
                bool file_exists = true;

                // Check if this path can be translated to a VFS path via mounts
                auto vfs_path_opt = vfs.mapFromHostPath(file_path);

                if(vfs_path_opt.has_value()) {
                    // Path is mounted in VFS, read via VFS
                    try {
                        content = vfs.read(vfs_path_opt.value(), std::nullopt);
                    } catch(...) {
                        file_exists = false;
                        content = ""; // New file
                    }
                } else {
                    // Not a mounted path, try VFS directly, fallback to host filesystem
                    try {
                        content = vfs.read(file_path, std::nullopt);
                    } catch(...) {
                        // Last resort: try reading from host filesystem
                        std::ifstream host_file(file_path);
                        if(host_file.good()) {
                            std::stringstream buffer;
                            buffer << host_file.rdbuf();
                            content = buffer.str();
                            host_file.close();
                        } else {
                            file_exists = false;
                            content = ""; // New file
                        }
                    }
                }
                
                // Split content into lines
                std::vector<std::string> lines;
                std::istringstream iss(content);
                std::string line;
                while(std::getline(iss, line)) {
                    lines.push_back(line);
                }
                
                // If file was empty but we have one empty line, clear it
                if(content.empty() && lines.size() == 1 && lines[0].empty()) {
                    lines.clear();
                }
                
                // If no lines, start with one empty line
                if(lines.empty()) {
                    lines.push_back("");
                }
                
#ifdef CODEX_UI_NCURSES
                // Use ncurses-based editor
                result.success = run_ncurses_editor(vfs, file_path, lines, file_exists, cwd.primary_overlay);
#else
                // Use simple terminal-based editor (fallback)
                result.success = run_simple_editor(vfs, file_path, lines, file_exists, cwd.primary_overlay);
#endif
            }

        } else if(cmd == "upp.wksp.file.cat") {
            // Print the active file content
            if (g_active_file_path.empty()) {
                throw std::runtime_error("No active file set. Use 'upp.wksp.file.set <file-path>' first.");
            }

            std::string file_path = g_active_file_path;
            std::string content;
            bool file_read = false;

            std::cout << "Active file path: " << file_path << "\n";
            std::cout << "Attempting to read file...\n";

            // Check if this path can be translated to a VFS path via mounts
            auto vfs_path_opt = vfs.mapFromHostPath(file_path);

            if(vfs_path_opt.has_value()) {
                std::cout << "Mapped to VFS path: " << vfs_path_opt.value() << "\n";
                // Path is mounted in VFS, read via VFS
                try {
                    content = vfs.read(vfs_path_opt.value(), std::nullopt);
                    std::cout << "Successfully read from VFS mount\n";
                    file_read = true;
                } catch(const std::exception& e) {
                    std::cout << "Failed to read from VFS: " << e.what() << "\n";
                }
            } else {
                std::cout << "Path not mapped to VFS mount\n";
            }

            if(!file_read) {
                // Try VFS directly
                try {
                    content = vfs.read(file_path, std::nullopt);
                    std::cout << "Successfully read from VFS directly\n";
                    file_read = true;
                } catch(const std::exception& e) {
                    std::cout << "Failed to read from VFS directly: " << e.what() << "\n";
                }
            }

            if(!file_read) {
                // Last resort: try reading from host filesystem
                std::ifstream host_file(file_path);
                if(host_file.good()) {
                    std::stringstream buffer;
                    buffer << host_file.rdbuf();
                    content = buffer.str();
                    host_file.close();
                    std::cout << "Successfully read from host filesystem\n";
                    file_read = true;
                } else {
                    std::cout << "Failed to read from host filesystem\n";
                }
            }

            if(file_read) {
                std::cout << "\n===== File Content =====\n";
                std::cout << content;
                if(!content.empty() && content.back() != '\n') {
                    std::cout << "\n";
                }
                std::cout << "===== End of File =====\n";
                std::cout << "Content size: " << content.size() << " bytes\n";
            } else {
                throw std::runtime_error("Could not read file from any source: " + file_path);
            }

        } else if(cmd == "upp.wksp.build") {
            if(!g_current_assembly) {
                throw std::runtime_error("No active workspace. Use 'upp.wksp.open' first.");
            }

            auto print_usage = [](){
                std::cout << "Usage: upp.wksp.build [options]\n"
                             "  --builder, -b <name>     Use a specific U++ build method (.bm id)\n"
                             "  --target,  -t <package>  Build a specific package instead of the primary\n"
                             "  --build-type <type>      Build configuration (debug|release, default debug)\n"
                             "  --output,  -o <path>     Output directory for artifacts\n"
                             "  --include, -I <path>     Additional include/search directory (repeatable)\n"
                             "  --dry-run                Show commands without executing them\n"
                             "  --plan                   Display build plan (implies --dry-run)\n"
                             "  --verbose, -v            Show all build commands executed (clang++, umk, etc.)\n"
                             "  --help,    -h            Show this help message\n";
            };

            WorkspaceBuildOptions build_opts;
            bool show_plan = false;
            bool show_help = false;

            for(size_t i = 0; i < inv.args.size(); ++i) {
                const std::string& arg = inv.args[i];
                if(arg == "--builder" || arg == "-b") {
                    if(i + 1 >= inv.args.size()) {
                        throw std::runtime_error("upp.wksp.build: --builder requires an argument");
                    }
                    build_opts.builder_name = inv.args[++i];
                } else if(arg == "--target" || arg == "-t") {
                    if(i + 1 >= inv.args.size()) {
                        throw std::runtime_error("upp.wksp.build: --target requires a package name");
                    }
                    build_opts.target_package = inv.args[++i];
                } else if(arg == "--build-type" || arg == "--mode") {
                    if(i + 1 >= inv.args.size()) {
                        throw std::runtime_error("upp.wksp.build: --build-type requires an argument");
                    }
                    build_opts.build_type = inv.args[++i];
                } else if(arg == "--output" || arg == "-o") {
                    if(i + 1 >= inv.args.size()) {
                        throw std::runtime_error("upp.wksp.build: --output requires a path");
                    }
                    build_opts.output_dir = inv.args[++i];
                } else if(arg == "--include" || arg == "-I") {
                    if(i + 1 >= inv.args.size()) {
                        throw std::runtime_error("upp.wksp.build: --include requires a path");
                    }
                    build_opts.extra_includes.push_back(inv.args[++i]);
                } else if(arg == "--dry-run") {
                    build_opts.dry_run = true;
                } else if(arg == "--plan") {
                    build_opts.dry_run = true;
                    show_plan = true;
                } else if(arg == "--verbose" || arg == "-v") {
                    build_opts.verbose = true;
                } else if(arg == "--help" || arg == "-h") {
                    print_usage();
                    show_help = true;
                } else {
                    throw std::runtime_error("upp.wksp.build: unknown option: " + arg);
                }
            }

            if(!show_help) {
                auto summary = build_workspace(*g_current_assembly, vfs, build_opts);

                std::cout << "upp.wksp.build: builder=" << summary.builder_used
                          << " type=" << build_opts.build_type
                          << (build_opts.dry_run ? " [dry-run]" : "")
                          << "\n";

                if(!summary.package_order.empty()) {
                    std::cout << "upp.wksp.build: packages (" << summary.package_order.size() << ")\n";
                    for(const auto& name : summary.package_order) {
                        std::cout << "  - " << name << "\n";
                    }
                }

                if(show_plan) {
                    std::cout << "upp.wksp.build: plan\n";
                    for(const auto& name : summary.package_order) {
                        auto target = "pkg:" + name;
                        auto it = summary.plan.rules.find(target);
                        if(it == summary.plan.rules.end()) continue;
                        std::cout << "  [" << name << "]\n";
                        for(const auto& cmd_entry : it->second.commands) {
                            std::cout << "    " << cmd_entry.text << "\n";
                        }
                    }
                }

                // Show build commands in verbose mode
                if(build_opts.verbose && !show_plan) {
                    std::cout << "upp.wksp.build: commands\n";
                    for(const auto& name : summary.package_order) {
                        auto target = "pkg." + name;
                        auto it = summary.plan.rules.find(target);
                        if(it == summary.plan.rules.end()) continue;
                        std::cout << "  [" << name << "]\n";
                        for(const auto& cmd_entry : it->second.commands) {
                            std::cout << "    " << cmd_entry.text << "\n";
                        }
                    }
                }

                if(!summary.result.output.empty()) {
                    std::cout << summary.result.output;
                }

                if(summary.result.success) {
                    if(summary.result.targets_built.empty()) {
                        std::cout << "upp.wksp.build: target up to date\n";
                    } else {
                        std::cout << "upp.wksp.build: built " << summary.result.targets_built.size() << " target(s)\n";

                        // Print output paths for each package
                        for(const auto& pkg_name : summary.package_order) {
                            auto it = summary.package_outputs.find(pkg_name);
                            if(it != summary.package_outputs.end()) {
                                const std::string& host_path = it->second;

                                // Try to map back to VFS path
                                auto vfs_path_opt = vfs.mapFromHostPath(host_path);

                                std::cout << "  " << pkg_name << ":\n";
                                std::cout << "    Host: " << host_path << "\n";
                                if(vfs_path_opt.has_value()) {
                                    std::cout << "    VFS:  " << vfs_path_opt.value() << "\n";
                                }
                            }
                        }
                    }
                } else {
                    if(!summary.result.errors.empty()) {
                        for(const auto& msg : summary.result.errors) {
                            std::cout << "upp.wksp.build: error: " << msg << "\n";
                        }
                    } else {
                        std::cout << "upp.wksp.build: build failed\n";
                    }
                    result.success = false;
                }
            }

        } else if(cmd == "make"){
            // Minimal GNU make implementation
            // Usage: make [target] [-f makefile] [-v|--verbose]
            std::string makefile_path = "/Makefile";
            std::string target = "all";  // Default target (first rule if not specified)
            bool verbose = false;

            // Parse arguments
            for(size_t i = 0; i < inv.args.size(); ++i){
                const std::string& arg = inv.args[i];
                if(arg == "-f" && i + 1 < inv.args.size()){
                    makefile_path = inv.args[++i];
                } else if(arg == "-v" || arg == "--verbose"){
                    verbose = true;
                } else {
                    target = arg;  // Assume it's a target name
                }
            }

            // Normalize makefile path
            makefile_path = normalize_path(cwd.path, makefile_path);

            // Read Makefile from VFS
            std::string makefile_content;
            try {
                makefile_content = vfs.read(makefile_path, std::nullopt);
            } catch(...) {
                throw std::runtime_error("make: Cannot read " + makefile_path);
            }

            MakeFile makefile;
            try {
                makefile.parse(makefile_content);
            } catch(const std::exception& e) {
                throw std::runtime_error(std::string("make: Parse error: ") + e.what());
            }

            // If target is "all" and not defined, use first rule
            if(target == "all" && !makefile.hasRule("all")){
                std::string fallback = makefile.firstRule();
                if(!fallback.empty()){
                    target = fallback;
                }
            }

            // Build target
            auto build_result = makefile.build(target, vfs, verbose);

            // Display output
            if(!build_result.output.empty()){
                std::cout << build_result.output;
            }

            // Display results
            if(build_result.success){
                if(build_result.targets_built.empty()){
                    std::cout << "make: '" << target << "' is up to date.\n";
                } else {
                    std::cout << "make: Successfully built " << build_result.targets_built.size() << " target(s)\n";
                }
            } else {
                // Display errors
                std::cout << "make: *** ";
                if(!build_result.errors.empty()){
                    std::cout << build_result.errors[0];
                } else {
                    std::cout << "Build failed";
                }
                std::cout << "\n";
                result.success = false;
            }

        } else if(cmd == "sample.run"){
            // Parse flags
            bool keep_temp = false;
            bool trace = false;
            for(const auto& arg : inv.args){
                if(arg == "--keep") keep_temp = true;
                else if(arg == "--trace") trace = true;
                else throw std::runtime_error("sample.run: unknown flag: " + arg);
            }

            auto start_time = std::chrono::steady_clock::now();

            // Step 1: Reset VFS state for deterministic runs
            if(trace) std::cout << "[sample.run] Resetting VFS state...\n";
            try { vfs.rm("/astcpp/demo", cwd.primary_overlay); } catch(...) {}
            try { vfs.rm("/cpp/demo.cpp", cwd.primary_overlay); } catch(...) {}
            try { vfs.rm("/logs/sample.compile.out", cwd.primary_overlay); } catch(...) {}
            try { vfs.rm("/logs/sample.compile.err", cwd.primary_overlay); } catch(...) {}
            try { vfs.rm("/logs/sample.run.out", cwd.primary_overlay); } catch(...) {}
            try { vfs.rm("/logs/sample.run.err", cwd.primary_overlay); } catch(...) {}
            try { vfs.rm("/env/sample.status", cwd.primary_overlay); } catch(...) {}

            // Step 2: Build demo translation unit using C++ AST helpers
            if(trace) std::cout << "[sample.run] Building C++ AST...\n";

            // Create /logs directory if it doesn't exist
            try { vfs.mkdir("/logs", cwd.primary_overlay); } catch(...) {}

            // Execute cpp commands to build the demo
            execute_single(CommandInvocation{"cpp.tu", {"/astcpp/demo"}}, "");
            execute_single(CommandInvocation{"cpp.include", {"/astcpp/demo", "iostream", "1"}}, "");
            execute_single(CommandInvocation{"cpp.func", {"/astcpp/demo", "main", "int"}}, "");
            execute_single(CommandInvocation{"cpp.print", {"/astcpp/demo/main", "Hello from codex-mini sample!"}}, "");
            execute_single(CommandInvocation{"cpp.returni", {"/astcpp/demo/main", "0"}}, "");
            execute_single(CommandInvocation{"cpp.dump", {"/astcpp/demo", "/cpp/demo.cpp"}}, "");

            // Read generated source
            std::string source_code = vfs.read("/cpp/demo.cpp", std::nullopt);

            // Step 3: Locate compiler
            std::string compiler = "c++";
            try {
                std::string env_compiler = vfs.read("/env/compiler", std::nullopt);
                if(!env_compiler.empty()) compiler = env_compiler;
            } catch(...) {}

            const char* env_cxx = std::getenv("CXX");
            if(env_cxx && env_cxx[0]) compiler = env_cxx;

            if(trace) std::cout << "[sample.run] Using compiler: " << compiler << "\n";

            // Step 4: Write source to temporary file and compile
            std::string temp_src = "/tmp/codex_sample_" + std::to_string(getpid()) + ".cpp";
            std::string temp_bin = "/tmp/codex_sample_" + std::to_string(getpid());

            {
                std::ofstream src_file(temp_src);
                if(!src_file) throw std::runtime_error("sample.run: cannot create temp source file");
                src_file << source_code;
            }

            if(trace) std::cout << "[sample.run] Compiling " << temp_src << " -> " << temp_bin << "\n";

            std::string compile_cmd = compiler + " -std=c++17 -O2 " + temp_src + " -o " + temp_bin + " 2>&1";
            FILE* compile_pipe = popen(compile_cmd.c_str(), "r");
            if(!compile_pipe) throw std::runtime_error("sample.run: cannot run compiler");

            std::string compile_output;
            char buffer[256];
            while(fgets(buffer, sizeof(buffer), compile_pipe)){
                compile_output += buffer;
            }
            int compile_status = pclose(compile_pipe);

            vfs.write("/logs/sample.compile.out", compile_output, cwd.primary_overlay);
            vfs.write("/logs/sample.compile.err", compile_status != 0 ? compile_output : "", cwd.primary_overlay);

            if(compile_status != 0){
                if(!keep_temp){
                    unlink(temp_src.c_str());
                }
                std::string status = "FAILED: compilation\nexit_code: " + std::to_string(WEXITSTATUS(compile_status)) + "\n";
                vfs.write("/env/sample.status", status, cwd.primary_overlay);
                std::cout << "sample.run: compilation failed (exit code " << WEXITSTATUS(compile_status) << ")\n";
                std::cout << "Logs: /logs/sample.compile.out, /logs/sample.compile.err\n";
                result.success = false;
                return result;
            }

            // Step 5: Execute compiled binary
            if(trace) std::cout << "[sample.run] Executing " << temp_bin << "\n";

            std::string exec_cmd = temp_bin + std::string(" 2>&1");
            FILE* exec_pipe = popen(exec_cmd.c_str(), "r");
            if(!exec_pipe){
                if(!keep_temp){
                    unlink(temp_src.c_str());
                    unlink(temp_bin.c_str());
                }
                throw std::runtime_error("sample.run: cannot execute binary");
            }

            std::string exec_output;
            while(fgets(buffer, sizeof(buffer), exec_pipe)){
                exec_output += buffer;
            }
            int exec_status = pclose(exec_pipe);

            vfs.write("/logs/sample.run.out", exec_output, cwd.primary_overlay);
            vfs.write("/logs/sample.run.err", exec_status != 0 ? exec_output : "", cwd.primary_overlay);

            // Step 6: Clean up temp files unless --keep
            if(!keep_temp){
                unlink(temp_src.c_str());
                unlink(temp_bin.c_str());
            } else {
                std::cout << "Kept temp files: " << temp_src << ", " << temp_bin << "\n";
            }

            // Step 7: Write status and timing
            auto end_time = std::chrono::steady_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

            std::ostringstream status_oss;
            if(exec_status == 0){
                status_oss << "SUCCESS\n";
            } else {
                status_oss << "FAILED: execution\n";
            }
            status_oss << "compile_exit_code: 0\n";
            status_oss << "exec_exit_code: " << WEXITSTATUS(exec_status) << "\n";
            status_oss << "duration_ms: " << duration_ms << "\n";

            vfs.write("/env/sample.status", status_oss.str(), cwd.primary_overlay);

            // Output summary
            std::cout << "sample.run: " << (exec_status == 0 ? "SUCCESS" : "FAILED") << "\n";
            std::cout << "Execution time: " << duration_ms << " ms\n";
            std::cout << "Output: /logs/sample.run.out\n";
            std::cout << "Status: /env/sample.status\n";

            if(exec_status != 0){
                result.success = false;
            }

        } else if(cmd == "upp.builder.load") {
            bool use_host_path = false;
            std::string dir_arg;
            for(const auto& arg : inv.args){
                if(arg == "-H") use_host_path = true;
                else dir_arg = arg;
            }
            if(dir_arg.empty()){
                throw std::runtime_error("upp.builder.load <directory-path> [-H]");
            }

            std::vector<std::string> loaded_builders;
            std::vector<std::string> errors;

            if(use_host_path){
                std::filesystem::path host_dir(dir_arg);
                if(!std::filesystem::exists(host_dir)){
                    throw std::runtime_error("upp.builder.load: host directory does not exist: " + host_dir.string());
                }
                if(!std::filesystem::is_directory(host_dir)){
                    throw std::runtime_error("upp.builder.load: not a directory: " + host_dir.string());
                }

                for(const auto& entry : std::filesystem::directory_iterator(host_dir)){
                    if(!entry.is_regular_file()) continue;
                    auto filename = entry.path().filename().string();
                    if(!has_bm_extension(filename)) continue;

                    std::ifstream file(entry.path());
                    if(!file){
                        errors.push_back(entry.path().string() + ": cannot open");
                        continue;
                    }
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    std::string parse_error;
                    std::string builder_id = entry.path().stem().string();
                    if(g_upp_builder_registry.parseAndStore(builder_id, entry.path().string(), buffer.str(), parse_error)){
                        loaded_builders.push_back(builder_id);
                    } else {
                        errors.push_back(entry.path().string() + ": " + parse_error);
                    }
                }
            } else {
                std::string dir_path = normalize_path(cwd.path, dir_arg);
                auto hits = vfs.resolveMulti(dir_path);
                if(hits.empty()){
                    throw std::runtime_error("upp.builder.load: directory not found: " + dir_path);
                }
                bool is_dir = false;
                for(const auto& hit : hits){
                    if(hit.node->isDir()){
                        is_dir = true;
                        break;
                    }
                }
                if(!is_dir){
                    throw std::runtime_error("upp.builder.load: not a directory: " + dir_path);
                }

                auto overlay_ids = vfs.overlaysForPath(dir_path);
                auto listing = vfs.listDir(dir_path, overlay_ids);
                for(const auto& [name, entry] : listing){
                    if(entry.types.count('f') == 0) continue;
                    if(!has_bm_extension(name)) continue;
                    std::string file_path = join_path(dir_path, name);
                    std::string content;
                    try {
                        content = vfs.read(file_path, std::nullopt);
                    } catch(const std::exception& e) {
                        errors.push_back(file_path + ": read failed (" + e.what() + ")");
                        continue;
                    } catch(...) {
                        errors.push_back(file_path + ": read failed (unknown error)");
                        continue;
                    }
                    std::string parse_error;
                    std::string builder_id = std::filesystem::path(name).stem().string();
                    if(g_upp_builder_registry.parseAndStore(builder_id, file_path, content, parse_error)){
                        loaded_builders.push_back(builder_id);
                    } else {
                        errors.push_back(file_path + ": " + parse_error);
                    }
                }
            }

            if(loaded_builders.empty()){
                std::cout << "upp.builder.load: no .bm files processed\n";
            } else {
                std::cout << "upp.builder.load: loaded " << loaded_builders.size() << " build method(s):\n";
                for(const auto& name : loaded_builders){
                    const auto* builder = g_upp_builder_registry.get(name);
                    std::cout << "  - " << name;
                    if(builder && !builder->builder_type.empty() && builder->builder_type != name){
                        std::cout << " [" << builder->builder_type << "]";
                    }
                    std::cout << "\n";
                }
            }
            if(!errors.empty()){
                std::cout << "upp.builder.load: encountered " << errors.size() << " issue(s):\n";
                for(const auto& msg : errors){
                    std::cout << "  ! " << msg << "\n";
                }
            }

        } else if(cmd == "upp.builder.add") {
            bool use_host_path = false;
            std::string path_arg;
            for(const auto& arg : inv.args){
                if(arg == "-H") use_host_path = true;
                else path_arg = arg;
            }
            if(path_arg.empty()){
                throw std::runtime_error("upp.builder.add <bm-file-path> [-H]");
            }

            std::string builder_id;
            std::string source_path;
            std::string content;
            if(use_host_path){
                std::filesystem::path host_file(path_arg);
                if(!std::filesystem::exists(host_file)){
                    throw std::runtime_error("upp.builder.add: host file does not exist: " + host_file.string());
                }
                if(!std::filesystem::is_regular_file(host_file)){
                    throw std::runtime_error("upp.builder.add: not a file: " + host_file.string());
                }
                if(!has_bm_extension(host_file.filename().string())){
                    throw std::runtime_error("upp.builder.add: expected .bm file");
                }
                std::ifstream file(host_file);
                if(!file){
                    throw std::runtime_error("upp.builder.add: cannot open " + host_file.string());
                }
                std::stringstream buffer;
                buffer << file.rdbuf();
                content = buffer.str();
                builder_id = host_file.stem().string();
                source_path = host_file.string();
            } else {
                std::string vfs_path = normalize_path(cwd.path, path_arg);
                if(!has_bm_extension(std::filesystem::path(vfs_path).filename().string())){
                    throw std::runtime_error("upp.builder.add: expected .bm file");
                }
                try {
                    content = vfs.read(vfs_path, std::nullopt);
                } catch(const std::exception& e) {
                    throw std::runtime_error("upp.builder.add: read failed: " + std::string(e.what()));
                }
                builder_id = std::filesystem::path(vfs_path).stem().string();
                source_path = vfs_path;
            }

            std::string parse_error;
            if(!g_upp_builder_registry.parseAndStore(builder_id, source_path, content, parse_error)){
                throw std::runtime_error("upp.builder.add: parse error: " + parse_error);
            }
            std::cout << "upp.builder.add: loaded build method '" << builder_id << "'\n";

        } else if(cmd == "upp.builder.list") {
            auto names = g_upp_builder_registry.list();
            if(names.empty()){
                std::cout << "No build methods loaded. Use 'upp.builder.load' or 'upp.builder.add'.\n";
            } else {
                auto active_name = g_upp_builder_registry.activeName();
                std::cout << "Loaded build methods (" << names.size() << "):\n";
                for(const auto& name : names){
                    const auto* builder = g_upp_builder_registry.get(name);
                    bool is_active = active_name && *active_name == name;
                    std::cout << (is_active ? "* " : "  ") << name;
                    if(builder && !builder->builder_type.empty() && builder->builder_type != name){
                        std::cout << " [" << builder->builder_type << "]";
                    }
                    if(builder && !builder->source_path.empty()){
                        std::cout << " <" << builder->source_path << ">";
                    }
                    if(is_active){
                        std::cout << " (active)";
                    }
                    std::cout << "\n";
                }
            }

        } else if(cmd == "upp.builder.active.set") {
            if(inv.args.empty()){
                throw std::runtime_error("upp.builder.active.set <builder-name>");
            }
            std::string builder_name = inv.args[0];
            std::string error_message;
            if(!g_upp_builder_registry.setActive(builder_name, error_message)){
                throw std::runtime_error("upp.builder.active.set: " + error_message);
            }
            std::cout << "Active build method set to '" << builder_name << "'\n";

        } else if(cmd == "upp.builder.get") {
            if(inv.args.empty()){
                throw std::runtime_error("upp.builder.get <key>");
            }
            auto* builder = g_upp_builder_registry.active();
            if(!builder){
                throw std::runtime_error("upp.builder.get: no active build method");
            }
            std::string key = inv.args[0];
            std::string key_upper = key;
            std::transform(key_upper.begin(), key_upper.end(), key_upper.begin(),
                           [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
            auto value = builder->get(key_upper);
            if(!value){
                throw std::runtime_error("upp.builder.get: key not found: " + key_upper);
            }
            std::cout << key_upper << " = \"" << *value << "\"\n";

        } else if(cmd == "upp.builder.set") {
            if(inv.args.size() < 2){
                throw std::runtime_error("upp.builder.set <key> <value>");
            }
            auto* builder = g_upp_builder_registry.active();
            if(!builder){
                throw std::runtime_error("upp.builder.set: no active build method");
            }
            std::string key = inv.args[0];
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
            std::string value = join_args(inv.args, 1);
            builder->set(key, value);
            std::cout << "Set " << key << " = \"" << value << "\" for builder '" << builder->id << "'\n";

        } else if(cmd == "upp.builder.dump") {
            if(inv.args.empty()){
                throw std::runtime_error("upp.builder.dump <builder-name>");
            }
            const auto* builder = g_upp_builder_registry.get(inv.args[0]);
            if(!builder){
                throw std::runtime_error("upp.builder.dump: builder not found: " + inv.args[0]);
            }
            dump_builder(*builder);

        } else if(cmd == "upp.builder.active.dump") {
            const auto* builder = g_upp_builder_registry.active();
            if(!builder){
                throw std::runtime_error("upp.builder.active.dump: no active build method");
            }
            dump_builder(*builder);

        } else if(cmd == "parse.file"){
            // libclang: parse C++ file from disk
            // Usage: parse.file <filepath> [vfs-target-path]
            try {
                cmd_parse_file(vfs, inv.args);
                std::cout << "Parsed C++ file successfully\n";
            } catch(const std::exception& e) {
                std::cout << "parse.file: " << e.what() << "\n";
                result.success = false;
            }

        } else if(cmd == "parse.dump"){
            // libclang: dump AST tree
            // Usage: parse.dump [vfs-path]
            try {
                cmd_parse_dump(vfs, inv.args);
            } catch(const std::exception& e) {
                std::cout << "parse.dump: " << e.what() << "\n";
                result.success = false;
            }

        } else if(cmd == "parse.generate"){
            // libclang: generate C++ code from AST
            // Usage: parse.generate <ast-path> <output-path>
            try {
                cmd_parse_generate(vfs, inv.args);
            } catch(const std::exception& e) {
                std::cout << "parse.generate: " << e.what() << "\n";
                result.success = false;
            }

        } else if(cmd == "edit" || cmd == "ee"){
            // Enhanced editor with UI backend abstraction
            // Check if we're in web server mode (no terminal available)
            if(web_server_mode) {
                result.success = false;
                result.output = "Interactive editor not available in web server mode. Use a text editor through the web interface or terminal.\n";
            } else {
                std::string vfs_path;
                bool has_filename = !inv.args.empty();
                
                if (has_filename) {
                    vfs_path = normalize_path(cwd.path, inv.args[0]);
                } else {
                    // No filename provided, start with an untitled buffer
                    std::cout << "VfsShell Text Editor (no file specified)\n";
                    std::cout << "Enter filename to save to later (or use :q to quit without saving):\n";
                    std::cout << "Filename: ";
                    std::getline(std::cin, vfs_path);
                    if (vfs_path.empty()) {
                        std::cout << "No filename provided. Editor closed.\n";
                        result.success = false;
                    } else {
                        // Normalize path if provided
                        vfs_path = normalize_path(cwd.path, vfs_path);
                    }
                }
                
                if (!vfs_path.empty()) {
                    // Read existing content if file exists
                    std::string content;
                    bool file_exists = true;
                    try {
                        content = vfs.read(vfs_path, std::nullopt);
                    } catch(...) {
                        file_exists = false;
                        content = ""; // New file
                    }
                    
                    // Split content into lines
                    std::vector<std::string> lines;
                    std::istringstream iss(content);
                    std::string line;
                    while(std::getline(iss, line)) {
                        lines.push_back(line);
                    }
                    
                    // If file was empty but we have one empty line, clear it
                    if(content.empty() && lines.size() == 1 && lines[0].empty()) {
                        lines.clear();
                    }
                    
                    // If no lines, start with one empty line
                    if(lines.empty()) {
                        lines.push_back("");
                    }
                    
#ifdef CODEX_UI_NCURSES
                    // Use ncurses-based editor
                    result.success = run_ncurses_editor(vfs, vfs_path, lines, file_exists, cwd.primary_overlay);
#else
                    // Use simple terminal-based editor (fallback)
                    result.success = run_simple_editor(vfs, vfs_path, lines, file_exists, cwd.primary_overlay);
#endif
                }
            }

        } else if(cmd == "help"){
            help();

        } else if(cmd == "quit" || cmd == "exit"){
            result.exit_requested = true;

        } else if(cmd.empty()){
            // nothing

        } else {
            std::cout << i18n::get(i18n::MsgId::UNKNOWN_COMMAND) << "\n";
            result.success = false;
        }

        result.output += capture.str();
        return result;
    };

    auto run_pipeline = [&](const CommandPipeline& pipeline, const std::string& initial_input) -> CommandResult {
        if(pipeline.commands.empty()) return CommandResult{};
        CommandResult last;
        std::string next_input = initial_input;
        for(size_t i = 0; i < pipeline.commands.size(); ++i){
            last = execute_single(pipeline.commands[i], next_input);
            if(last.exit_requested) return last;
            next_input = last.output;
        }

        // Handle output redirection
        if(!pipeline.output_redirect.empty()){
            std::string abs_path = normalize_path(cwd.path, pipeline.output_redirect);
            if(pipeline.redirect_append){
                // >> append to file
                std::string existing;
                try {
                    existing = vfs.read(abs_path, std::nullopt);
                } catch(...) {
                    // File doesn't exist yet, that's ok
                }
                vfs.write(abs_path, existing + last.output, cwd.primary_overlay);
            } else {
                // > overwrite file
                vfs.write(abs_path, last.output, cwd.primary_overlay);
            }
            last.output.clear(); // don't print to stdout when redirected
        }

        return last;
    };

    // Register command callback for web server
    if(web_server_mode){
        WebServer::set_command_callback([&](const std::string& command_line) -> std::pair<bool, std::string> {
            try {
                auto tokens = tokenize_command_line(command_line);
                if(tokens.empty()) return {true, ""};

                // Skip comment lines
                if(!tokens.empty() && !tokens[0].empty() && tokens[0][0] == '#') {
                    return {true, ""};
                }

                auto chain = parse_command_chain(tokens);
                bool last_success = true;
                std::string combined_output;

                for(const auto& entry : chain){
                    if(entry.logical == "&&" && !last_success) continue;
                    if(entry.logical == "||" && last_success) continue;

                    CommandResult res = run_pipeline(entry.pipeline, "");
                    if(!res.output.empty()){
                        combined_output += res.output;
                    }
                    last_success = res.success;

                    if(res.exit_requested){
                        // Client requested exit - could handle this specially
                        break;
                    }
                }

                return {last_success, combined_output};
            } catch(const std::exception& e){
                return {false, std::string("error: ") + e.what() + "\r\n"};
            }
        });

        std::cout << "Starting web server on port " << web_server_port << "...\n";
        if(!WebServer::start(web_server_port)){
            std::cerr << "Failed to start web server\n";
            return 1;
        }
        std::cout << "Web server running. Press Ctrl+C to stop.\n";
        std::cout << "Open browser to: http://localhost:" << web_server_port << "/\n";

        // Keep server running
        while(WebServer::is_running()){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        WebServer::stop();
        return 0;
    }

    // Load .vfshrc initialization file if present (first current directory, then home directory)
    try{
        bool vfshrc_loaded = false;
        
        // First check current directory for .vfshrc
        std::filesystem::path local_vfshrc_path = ".vfshrc";
        if(std::filesystem::exists(local_vfshrc_path) && std::filesystem::is_regular_file(local_vfshrc_path)){
            std::cout << "loading .vfshrc configuration...\n";
            // Read and execute the local .vfshrc file
            std::ifstream vfshrc_stream(local_vfshrc_path);
            std::string vfshrc_line;
            while(std::getline(vfshrc_stream, vfshrc_line)){
                auto trimmed = trim_copy(vfshrc_line);
                if(trimmed.empty() || trimmed[0] == '#') continue; // Skip empty lines and comments
                
                try{
                    auto tokens = tokenize_command_line(vfshrc_line);
                    if(tokens.empty()) continue;
                    
                    auto chain = parse_command_chain(tokens);
                    bool last_success = true;
                    
                    for(const auto& entry : chain){
                        if(entry.logical == "&&" && !last_success) continue;
                        if(entry.logical == "||" && last_success) continue;
                        
                        CommandResult res = run_pipeline(entry.pipeline, "");
                        if(!res.output.empty()){
                            std::cout << res.output;
                            std::cout.flush();
                        }
                        last_success = res.success;
                        
                        if(res.exit_requested){
                            break;
                        }
                    }
                } catch(const std::exception& e){
                    std::cout << "warning: .vfshrc error: " << e.what() << "\n";
                }
            }
            vfshrc_loaded = true;
        }
        
        // If not loaded from current directory, check home directory
        if(!vfshrc_loaded){
            const char* home = std::getenv("HOME");
            if(home != nullptr){
                std::filesystem::path home_vfshrc_path = std::string(home) + "/.vfshrc";
                if(std::filesystem::exists(home_vfshrc_path) && std::filesystem::is_regular_file(home_vfshrc_path)){
                    std::cout << "loading ~/.vfshrc configuration...\n";
                    // Read and execute the home .vfshrc file
                    std::ifstream vfshrc_stream(home_vfshrc_path);
                    std::string vfshrc_line;
                    while(std::getline(vfshrc_stream, vfshrc_line)){
                        auto trimmed = trim_copy(vfshrc_line);
                        if(trimmed.empty() || trimmed[0] == '#') continue; // Skip empty lines and comments
                        
                        try{
                            auto tokens = tokenize_command_line(vfshrc_line);
                            if(tokens.empty()) continue;
                            
                            auto chain = parse_command_chain(tokens);
                            bool last_success = true;
                            
                            for(const auto& entry : chain){
                                if(entry.logical == "&&" && !last_success) continue;
                                if(entry.logical == "||" && last_success) continue;
                                
                                CommandResult res = run_pipeline(entry.pipeline, "");
                                if(!res.output.empty()){
                                    std::cout << res.output;
                                    std::cout.flush();
                                }
                                last_success = res.success;
                                
                                if(res.exit_requested){
                                    break;
                                }
                            }
                        } catch(const std::exception& e){
                            std::cout << "warning: ~/.vfshrc error: " << e.what() << "\n";
                        }
                    }
                }
            }
        }
    } catch(const std::exception& e){
        std::cout << "note: loading .vfshrc failed: " << e.what() << "\n";
    }

    while(true){
        TRACE_LOOP("repl.iter", std::string("iter=") + std::to_string(repl_iter));
        ++repl_iter;
        bool have_line = false;
        if(interactive && input == &std::cin){
            if(!read_line_with_history(vfs, "> ", line, history, cwd.path)){
                break;
            }
            have_line = true;
        } else {
            if(!std::getline(*input, line)){
                if(script_active && fallback_after_script){
                    script_active = false;
                    fallback_after_script = false;
                    scriptStream.reset();
                    input = &std::cin;
                    interactive = true;
                    if(!std::cin.good()) std::cin.clear();
                    continue;
                }
                break;
            }
            have_line = true;
        }

        if(!have_line) break;

        auto trimmed = trim_copy(line);
        if(trimmed.empty()) continue;
        // Skip comment lines starting with #
        if(!trimmed.empty() && trimmed[0] == '#') continue;
        try{
            auto tokens = tokenize_command_line(line);
            if(tokens.empty()) continue;
            bool simple_history = false;
            if(tokens[0] == "history"){
                simple_history = true;
                for(const auto& tok : tokens){
                    if(tok == "|" || tok == "&&" || tok == "||"){ simple_history = false; break; }
                }
            }
            bool record_line = !simple_history;
            if(record_line){
                history.push_back(line);
                history_dirty = true;
            }
            auto chain = parse_command_chain(tokens);
            bool exit_requested = false;
            bool last_success = true;
            for(const auto& entry : chain){
                if(entry.logical == "&&" && !last_success) continue;
                if(entry.logical == "||" && last_success) continue;
                CommandResult res = run_pipeline(entry.pipeline, "");
                if(!res.output.empty()){
                    std::cout << res.output;
                    std::cout.flush();
                }
                last_success = res.success;
                if(res.exit_requested){
                    exit_requested = true;
                    break;
                }
            }
            if(exit_requested) break;
        } catch(const std::exception& e){
            std::cout << "error: " << e.what() << "\n";
        }
    }
    if(solution.active && vfs.overlayDirty(solution.overlay_id)){
        while(true){
            std::cout << "Solution '" << solution.title << "' modified. Save changes? [y/N] ";
            std::string answer;
            if(!std::getline(std::cin, answer)){
                std::cout << '\n';
                break;
            }
            std::string trimmed = trim_copy(answer);
            if(trimmed.empty()){
                break;
            }
            char c = static_cast<char>(std::tolower(static_cast<unsigned char>(trimmed[0])));
            if(c == 'y'){
                solution_save(vfs, solution, false);
                break;
            }
            if(c == 'n'){
                break;
            }
            std::cout << "Please answer y or n.\n";
        }
    }
    if(history_dirty) save_history(history);
    return 0;
}

// ============================================================================
// Enhanced Editor Implementation with UI Backend Abstraction
// ============================================================================

// Forward declarations for editor functions
bool run_ncurses_editor(Vfs& vfs, const std::string& vfs_path, std::vector<std::string>& lines, 
                       bool file_exists, size_t overlay_id);

bool run_simple_editor(Vfs& vfs, const std::string& vfs_path, std::vector<std::string>& lines, 
                      bool file_exists, size_t overlay_id) {
	return false;
}

// NCURSES-based editor implementation
#ifdef CODEX_UI_NCURSES
// NCURSES-based editor implementation
bool run_ncurses_editor(Vfs& vfs, const std::string& vfs_path, std::vector<std::string>& lines, 
                       bool file_exists, size_t overlay_id) {
    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    
    // Colors (if supported)
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_BLUE, COLOR_BLACK);   // Title bar
        init_pair(2, COLOR_YELLOW, COLOR_BLACK); // Status bar
        init_pair(3, COLOR_CYAN, COLOR_BLACK);    // Line numbers
        init_pair(4, COLOR_RED, COLOR_BLACK);     // Modified indicator
    }
    
    int current_line = 0;
    int top_line = 0;
    int cursor_x = 0;
    bool file_modified = false;
    bool editor_active = true;
    int content_height = rows - 4;  // Leave space for status bars
    
    // Main editor loop
    while (editor_active) {
        // Clear screen
        clear();
        
        // Draw title bar
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(0, 0, "VfsShell Text Editor - %s", vfs_path.c_str());
        attroff(COLOR_PAIR(1) | A_BOLD);
        
        // Draw separator
        mvhline(1, 0, '-', cols);
        
        // Draw content area
        for (int i = 0; i < content_height && (top_line + i) < static_cast<int>(lines.size()); ++i) {
            int line_idx = top_line + i;
            int screen_row = i + 2;
            
            // Line number
            attron(COLOR_PAIR(3));
            mvprintw(screen_row, 0, "%3d:", line_idx + 1);
            attroff(COLOR_PAIR(3));
            
            // Line content
            if (line_idx < static_cast<int>(lines.size())) {
                std::string display_line = lines[line_idx];
                // Truncate if too long
                if (static_cast<int>(display_line.length()) > cols - 6) {
                    display_line = display_line.substr(0, cols - 9) + "...";
                }
                mvprintw(screen_row, 5, "%s", display_line.c_str());
            }
        }
        
        // Fill remaining lines with tildes
        for (int i = lines.size() - top_line; i < content_height; ++i) {
            int screen_row = i + 2;
            if (screen_row < rows - 2) {
                mvprintw(screen_row, 0, "~");
            }
        }
        
        // Draw separator
        mvhline(rows - 2, 0, '-', cols);
        
        // Draw status bar
        attron(COLOR_PAIR(2));
        mvprintw(rows - 1, 0, "Line:%d/%d Col:%d | %s%s | :w (save) :q (quit) :wq (save&quit)",
                current_line + 1, static_cast<int>(lines.size()), cursor_x,
                file_modified ? "[Modified] " : "",
                !file_exists ? "[New File] " : "");
        attroff(COLOR_PAIR(2));
        
        // Position cursor
        if (current_line >= top_line && current_line < top_line + content_height) {
            move(current_line - top_line + 2, std::min(cursor_x + 5, cols - 1));
        }
        
        // Refresh screen
        refresh();
        
        // Get user input
        int ch = getch();
        
        // Process input
        switch (ch) {
            case KEY_UP:
                if (current_line > 0) {
                    current_line--;
                    if (current_line < top_line) {
                        top_line = current_line;
                    }
                    // Adjust cursor_x if needed
                    if (cursor_x > static_cast<int>(lines[current_line].length())) {
                        cursor_x = lines[current_line].length();
                    }
                }
                break;
                
            case KEY_DOWN:
                if (current_line < static_cast<int>(lines.size()) - 1) {
                    current_line++;
                    if (current_line >= top_line + content_height) {
                        top_line = current_line - content_height + 1;
                    }
                    // Adjust cursor_x if needed
                    if (cursor_x > static_cast<int>(lines[current_line].length())) {
                        cursor_x = lines[current_line].length();
                    }
                }
                break;
                
            case KEY_LEFT:
                if (cursor_x > 0) {
                    cursor_x--;
                } else if (current_line > 0) {
                    // Move to end of previous line
                    current_line--;
                    if (current_line < top_line) {
                        top_line = current_line;
                    }
                    cursor_x = lines[current_line].length();
                }
                break;
                
            case KEY_RIGHT:
                if (cursor_x < static_cast<int>(lines[current_line].length())) {
                    cursor_x++;
                } else if (current_line < static_cast<int>(lines.size()) - 1) {
                    // Move to beginning of next line
                    current_line++;
                    if (current_line >= top_line + content_height) {
                        top_line = current_line - content_height + 1;
                    }
                    cursor_x = 0;
                }
                break;
                
            case KEY_BACKSPACE:
            case 127:  // Delete key
            case '\b':
                if (cursor_x > 0) {
                    lines[current_line].erase(cursor_x - 1, 1);
                    cursor_x--;
                    file_modified = true;
                } else if (current_line > 0) {
                    // Join with previous line
                    std::string current_content = lines[current_line];
                    lines.erase(lines.begin() + current_line);
                    current_line--;
                    cursor_x = lines[current_line].length();
                    lines[current_line] += current_content;
                    file_modified = true;
                    
                    if (current_line < top_line) {
                        top_line = current_line;
                    }
                }
                break;
                
            case KEY_DC:  // Delete key
                if (cursor_x < static_cast<int>(lines[current_line].length())) {
                    lines[current_line].erase(cursor_x, 1);
                    file_modified = true;
                } else if (current_line < static_cast<int>(lines.size()) - 1) {
                    // Join with next line
                    std::string next_content = lines[current_line + 1];
                    lines[current_line] += next_content;
                    lines.erase(lines.begin() + current_line + 1);
                    file_modified = true;
                }
                break;
                
            case KEY_ENTER:
            case '\n':
            case '\r': {
                // Split line at cursor position
                std::string new_line = lines[current_line].substr(cursor_x);
                lines[current_line] = lines[current_line].substr(0, cursor_x);
                lines.insert(lines.begin() + current_line + 1, new_line);
                current_line++;
                cursor_x = 0;
                file_modified = true;
                
                if (current_line >= top_line + content_height) {
                    top_line = current_line - content_height + 1;
                }
                break;
            }
                
            case 27:  // ESC key - command mode
                {
                    // Show command prompt at bottom
                    move(rows - 1, 0);
                    clrtoeol();
                    attron(COLOR_PAIR(2));
                    printw(":");
                    attroff(COLOR_PAIR(2));
                    refresh();
                    
                    // Get command
                    echo();
                    char cmd[256];
                    getnstr(cmd, sizeof(cmd) - 1);
                    noecho();
                    
                    std::string command = cmd;
                    
                    if (command == "q") {
                        if (file_modified) {
                            // Show warning
                            move(rows - 1, 0);
                            clrtoeol();
                            attron(COLOR_PAIR(2) | A_BOLD);
                            printw("File modified. Use :wq to save or :q! to quit without saving.");
                            attroff(COLOR_PAIR(2) | A_BOLD);
                            refresh();
                            getch();  // Wait for keypress
                        } else {
                            editor_active = false;
                        }
                    } else if (command == "q!") {
                        editor_active = false;
                    } else if (command == "w") {
                        // Save file
                        std::ostringstream oss;
                        for (size_t i = 0; i < lines.size(); ++i) {
                            oss << lines[i];
                            if (i < lines.size() - 1) oss << "\n";
                        }
                        std::string new_content = oss.str();

                        // Check if this path can be translated to a VFS path via mounts
                        auto vfs_mapped_path = vfs.mapFromHostPath(vfs_path);

                        if (vfs_mapped_path.has_value()) {
                            // Path is mounted in VFS, write via VFS
                            try {
                                vfs.write(vfs_mapped_path.value(), new_content, overlay_id);
                                file_modified = false;
                            } catch(const std::exception& e) {
                                move(rows - 1, 0);
                                clrtoeol();
                                attron(COLOR_PAIR(2) | A_BOLD);
                                printw("Error: VFS write failed: %s", e.what());
                                attroff(COLOR_PAIR(2) | A_BOLD);
                                refresh();
                                sleep(2);
                            }
                        } else {
                            // Not a mounted path, try VFS directly, fallback to host filesystem
                            try {
                                vfs.write(vfs_path, new_content, overlay_id);
                                file_modified = false;
                            } catch(...) {
                                // Fallback: write to host filesystem
                                std::ofstream host_out(vfs_path);
                                if (host_out.good()) {
                                    host_out << new_content;
                                    host_out.close();
                                    file_modified = false;
                                } else {
                                    move(rows - 1, 0);
                                    clrtoeol();
                                    attron(COLOR_PAIR(2) | A_BOLD);
                                    printw("Error: Could not write file: %s", vfs_path.c_str());
                                    attroff(COLOR_PAIR(2) | A_BOLD);
                                    refresh();
                                    sleep(2);
                                }
                            }
                        }

                        if (!file_modified) {
                            // Show confirmation
                            move(rows - 1, 0);
                            clrtoeol();
                            attron(COLOR_PAIR(2));
                            printw("[Saved %d lines to %s]", static_cast<int>(lines.size()), vfs_path.c_str());
                            attroff(COLOR_PAIR(2));
                            refresh();
                            sleep(1);
                        }
                    } else if (command == "wq" || command == "x") {
                        // Save and quit
                        std::ostringstream oss;
                        for (size_t i = 0; i < lines.size(); ++i) {
                            oss << lines[i];
                            if (i < lines.size() - 1) oss << "\n";
                        }
                        std::string new_content = oss.str();

                        // Check if this path can be translated to a VFS path via mounts
                        auto vfs_mapped_path = vfs.mapFromHostPath(vfs_path);

                        if (vfs_mapped_path.has_value()) {
                            // Path is mounted in VFS, write via VFS
                            try {
                                vfs.write(vfs_mapped_path.value(), new_content, overlay_id);
                            } catch(...) {
                                // Ignore errors on quit
                            }
                        } else {
                            // Not a mounted path, try VFS directly, fallback to host filesystem
                            try {
                                vfs.write(vfs_path, new_content, overlay_id);
                            } catch(...) {
                                // Fallback: write to host filesystem
                                std::ofstream host_out(vfs_path);
                                if (host_out.good()) {
                                    host_out << new_content;
                                    host_out.close();
                                }
                            }
                        }
                        editor_active = false;
                    } else if (command == "help") {
                        // Show help
                        clear();
                        mvprintw(0, 0, "VfsShell Editor Help");
                        mvprintw(1, 0, "=====================");
                        mvprintw(2, 0, "Navigation:");
                        mvprintw(3, 2, "Arrow Keys - Move cursor");
                        mvprintw(4, 2, "ESC        - Enter command mode");
                        mvprintw(5, 0, "Editing:");
                        mvprintw(6, 2, "Type       - Insert text");
                        mvprintw(7, 2, "Backspace  - Delete character before cursor");
                        mvprintw(8, 2, "Delete     - Delete character at cursor");
                        mvprintw(9, 2, "Enter      - Insert new line");
                        mvprintw(10, 0, "Commands (in command mode):");
                        mvprintw(11, 2, ":w         - Save file");
                        mvprintw(12, 2, ":q         - Quit");
                        mvprintw(13, 2, ":q!        - Quit without saving");
                        mvprintw(14, 2, ":wq or :x  - Save and quit");
                        mvprintw(15, 2, ":help      - Show this help");
                        mvprintw(17, 0, "Press any key to continue...");
                        refresh();
                        getch();
                    } else if (!command.empty()) {
                        // Unknown command
                        move(rows - 1, 0);
                        clrtoeol();
                        attron(COLOR_PAIR(2) | A_BOLD);
                        printw("Unknown command: %s", command.c_str());
                        attroff(COLOR_PAIR(2) | A_BOLD);
                        refresh();
                        sleep(1);
                    }
                }
                break;
                
            default:
                // Regular character input
                if (ch >= 32 && ch <= 126) {  // Printable ASCII
                    lines[current_line].insert(cursor_x, 1, static_cast<char>(ch));
                    cursor_x++;
                    file_modified = true;
                }
                break;
        }
    }
    
    // Clean up ncurses
    endwin();
    
    return true;
}
#endif

#if 0
// Simple terminal-based editor (fallback implementation)
                } else {
                    std::cout << "[Error: line number out of range]\n";
                }
            } else {
                std::cout << "[Invalid change command]\n";
            }
        } else if(command == "p") {
            // Print current content
            std::cout << "\033[2J\033[H"; // Clear and show current state
            std::cout << "\033[34;1mVfsShell Text Editor - " << vfs_path << "\033[0m" << std::endl;
            std::cout << "\033[34m" << std::string(60, '=') << "\033[0m" << std::endl;
            
            for(size_t i = 0; i < lines.size(); ++i) {
                std::cout << std::right << std::setw(3) << (i + 1) << ": " << lines[i] << std::endl;
            }
            
            // Show more empty lines if needed
            for(size_t i = lines.size(); i < 10 && i < 20; ++i) {
                std::cout << std::right << std::setw(3) << (i + 1) << ": ~" << std::endl;
            }
            
            std::cout << "\033[34m" << std::string(60, '=') << "\033[0m" << std::endl;
            std::cout << "\033[33mStatus: " << lines.size() << " lines | ";
            if (!file_exists && !file_modified) std::cout << "[New File] | ";
            if (file_modified) std::cout << "[Modified] | ";
            std::cout << "Type :wq to save&quit, :q to quit, :help for commands\033[0m" << std::endl;
            std::cout << std::endl;
        } else if(command.empty()) {
            // Do nothing, just continue
        } else {
            std::cout << "[Unknown command. Type :help for options]\n";
        }
    }
    
    std::cout << "\033[2J\033[H"; // Clear screen on exit
    std::cout << "Editor closed. Return to shell.\n";
    
    return true;
}
#endif


#endif // CODEX_NO_MAIN
