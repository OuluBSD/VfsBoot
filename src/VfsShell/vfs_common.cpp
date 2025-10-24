#include "VfsShell.h"

//
// Internationalization implementation
//
namespace i18n {
    namespace {
        enum class Lang { EN, FI };
        Lang current_lang = Lang::EN;

        struct MsgTable {
            const char* en;
#ifdef CODEX_I18N_ENABLED
            const char* fi;
#endif
        };

        const MsgTable messages[] = {
            // WELCOME
            { "VfsShell 🌲 VFS+AST+AI — type 'help' for available commands."
#ifdef CODEX_I18N_ENABLED
            , "VfsShell 🌲 VFS+AST+AI — 'help' kertoo karun totuuden."
#endif
            },
            // UNKNOWN_COMMAND
            { "error: unknown command. Type 'help' for available commands."
#ifdef CODEX_I18N_ENABLED
            , "virhe: tuntematon komento. 'help' kertoo karun totuuden."
#endif
            },
            // DISCUSS_HINT
            { "💡 Tip: Use 'discuss' to work with AI on your code (natural language → plans → implementation)"
#ifdef CODEX_I18N_ENABLED
            , "💡 Vinkki: Käytä 'discuss' komentoa työskennelläksesi AI:n kanssa (luonnollinen kieli → suunnitelmat → toteutus)"
#endif
            },
        };

        Lang detect_language() {
            const char* lang_env = std::getenv("LANG");
            if(!lang_env) lang_env = std::getenv("LC_MESSAGES");
            if(!lang_env) lang_env = std::getenv("LC_ALL");

            if(lang_env) {
                std::string lang_str(lang_env);
                if(lang_str.find("fi_") == 0 || lang_str.find("fi.") == 0 ||
                   lang_str.find("finnish") != std::string::npos ||
                   lang_str.find("Finnish") != std::string::npos) {
                    return Lang::FI;
                }
            }
            return Lang::EN;
        }
    }

    void init() {
#ifdef CODEX_I18N_ENABLED
        current_lang = detect_language();
#else
        current_lang = Lang::EN;
#endif
    }

    void set_english_only() {
        current_lang = Lang::EN;
    }

    const char* get(MsgId id) {
        size_t idx = static_cast<size_t>(id);
        if(idx >= sizeof(messages) / sizeof(messages[0])) {
            return "??? missing translation ???";
        }
#ifdef CODEX_I18N_ENABLED
        if(current_lang == Lang::FI) {
            return messages[idx].fi;
        }
#endif
        return messages[idx].en;
    }
}


const char* policy_label(WorkingDirectory::ConflictPolicy policy){
    switch(policy){
        case WorkingDirectory::ConflictPolicy::Manual: return "manual";
        case WorkingDirectory::ConflictPolicy::Oldest: return "oldest";
        case WorkingDirectory::ConflictPolicy::Newest: return "newest";
    }
    return "?";
}

std::optional<WorkingDirectory::ConflictPolicy> parse_policy(const std::string& name){
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    if(lower == "manual" || lower == "default") return WorkingDirectory::ConflictPolicy::Manual;
    if(lower == "oldest" || lower == "first") return WorkingDirectory::ConflictPolicy::Oldest;
    if(lower == "newest" || lower == "last") return WorkingDirectory::ConflictPolicy::Newest;
    return std::nullopt;
}
