#include "VfsShell.h"

// ====== Planner Nodes ======
std::string PlanGoals::read() const {
    std::string result;
    for(size_t i = 0; i < goals.size(); ++i){
        result += "- " + goals[i] + "\n";
    }
    return result;
}

void PlanGoals::write(const std::string& s){
    goals.clear();
    std::istringstream iss(s);
    std::string line;
    while(std::getline(iss, line)){
        auto trimmed = trim_copy(line);
        if(trimmed.empty()) continue;
        if(trimmed.size() >= 2 && trimmed[0] == '-' && std::isspace(static_cast<unsigned char>(trimmed[1]))){
            goals.push_back(trim_copy(trimmed.substr(2)));
        } else {
            goals.push_back(trimmed);
        }
    }
}

std::string PlanIdeas::read() const {
    std::string result;
    for(size_t i = 0; i < ideas.size(); ++i){
        result += "- " + ideas[i] + "\n";
    }
    return result;
}

void PlanIdeas::write(const std::string& s){
    ideas.clear();
    std::istringstream iss(s);
    std::string line;
    while(std::getline(iss, line)){
        auto trimmed = trim_copy(line);
        if(trimmed.empty()) continue;
        if(trimmed.size() >= 2 && trimmed[0] == '-' && std::isspace(static_cast<unsigned char>(trimmed[1]))){
            ideas.push_back(trim_copy(trimmed.substr(2)));
        } else {
            ideas.push_back(trimmed);
        }
    }
}

std::string PlanJobs::read() const {
    std::string result;
    auto sorted = getSortedJobIndices();
    for(size_t idx : sorted){
        const auto& job = jobs[idx];
        result += (job.completed ? "[x] " : "[ ] ");
        result += "P" + std::to_string(job.priority) + " ";
        result += job.description;
        if(!job.assignee.empty()){
            result += " (@" + job.assignee + ")";
        }
        result += "\n";
    }
    return result;
}

void PlanJobs::write(const std::string& s){
    jobs.clear();
    std::istringstream iss(s);
    std::string line;
    while(std::getline(iss, line)){
        auto trimmed = trim_copy(line);
        if(trimmed.empty()) continue;

        PlanJob job;
        job.completed = false;
        job.priority = 100;
        job.assignee = "";

        // Parse [x] or [ ]
        if(trimmed.size() >= 3 && trimmed[0] == '['){
            if(trimmed[1] == 'x' || trimmed[1] == 'X'){
                job.completed = true;
            }
            size_t close = trimmed.find(']');
            if(close != std::string::npos && close < trimmed.size() - 1){
                trimmed = trim_copy(trimmed.substr(close + 1));
            }
        }

        // Parse priority P<num>
        if(trimmed.size() >= 2 && trimmed[0] == 'P' && std::isdigit(static_cast<unsigned char>(trimmed[1]))){
            size_t end = 1;
            while(end < trimmed.size() && std::isdigit(static_cast<unsigned char>(trimmed[end]))){
                ++end;
            }
            job.priority = std::stoi(trimmed.substr(1, end - 1));
            trimmed = trim_copy(trimmed.substr(end));
        }

        // Parse assignee (@name)
        size_t at_pos = trimmed.find(" (@");
        if(at_pos != std::string::npos){
            size_t close_paren = trimmed.find(')', at_pos);
            if(close_paren != std::string::npos){
                job.assignee = trimmed.substr(at_pos + 3, close_paren - at_pos - 3);
                trimmed = trim_copy(trimmed.substr(0, at_pos));
            }
        }

        job.description = trimmed;
        if(!job.description.empty()){
            jobs.push_back(job);
        }
    }
}

void PlanJobs::addJob(const std::string& desc, int priority, const std::string& assignee){
    PlanJob job;
    job.description = desc;
    job.priority = priority;
    job.completed = false;
    job.assignee = assignee;
    jobs.push_back(job);
}

void PlanJobs::completeJob(size_t index){
    if(index < jobs.size()){
        jobs[index].completed = true;
    }
}

std::vector<size_t> PlanJobs::getSortedJobIndices() const {
    std::vector<size_t> indices(jobs.size());
    for(size_t i = 0; i < jobs.size(); ++i) indices[i] = i;
    std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b){
        if(jobs[a].completed != jobs[b].completed) return !jobs[a].completed;  // Incomplete first
        if(jobs[a].priority != jobs[b].priority) return jobs[a].priority < jobs[b].priority;
        return a < b;
    });
    return indices;
}

std::string PlanDeps::read() const {
    std::string result;
    for(const auto& dep : dependencies){
        result += "- " + dep + "\n";
    }
    return result;
}

void PlanDeps::write(const std::string& s){
    dependencies.clear();
    std::istringstream iss(s);
    std::string line;
    while(std::getline(iss, line)){
        auto trimmed = trim_copy(line);
        if(trimmed.empty()) continue;
        if(trimmed.size() >= 2 && trimmed[0] == '-' && std::isspace(static_cast<unsigned char>(trimmed[1]))){
            dependencies.push_back(trim_copy(trimmed.substr(2)));
        } else {
            dependencies.push_back(trimmed);
        }
    }
}

std::string PlanImplemented::read() const {
    std::string result;
    for(const auto& item : items){
        result += "- " + item + "\n";
    }
    return result;
}

void PlanImplemented::write(const std::string& s){
    items.clear();
    std::istringstream iss(s);
    std::string line;
    while(std::getline(iss, line)){
        auto trimmed = trim_copy(line);
        if(trimmed.empty()) continue;
        if(trimmed.size() >= 2 && trimmed[0] == '-' && std::isspace(static_cast<unsigned char>(trimmed[1]))){
            items.push_back(trim_copy(trimmed.substr(2)));
        } else {
            items.push_back(trimmed);
        }
    }
}

std::string PlanResearch::read() const {
    std::string result;
    for(const auto& topic : topics){
        result += "- " + topic + "\n";
    }
    return result;
}

void PlanResearch::write(const std::string& s){
    topics.clear();
    std::istringstream iss(s);
    std::string line;
    while(std::getline(iss, line)){
        auto trimmed = trim_copy(line);
        if(trimmed.empty()) continue;
        if(trimmed.size() >= 2 && trimmed[0] == '-' && std::isspace(static_cast<unsigned char>(trimmed[1]))){
            topics.push_back(trim_copy(trimmed.substr(2)));
        } else {
            topics.push_back(trimmed);
        }
    }
}

// Planner Context
void PlannerContext::navigateTo(const std::string& path){
    if(!current_path.empty()){
        navigation_history.push_back(current_path);
    }
    current_path = path;
}

void PlannerContext::forward(){
    mode = Mode::Forward;
}

void PlannerContext::backward(){
    mode = Mode::Backward;
    if(!navigation_history.empty()){
        current_path = navigation_history.back();
        navigation_history.pop_back();
    }
}

void PlannerContext::addToContext(const std::string& vfs_path){
    visible_nodes.insert(vfs_path);
}

void PlannerContext::removeFromContext(const std::string& vfs_path){
    visible_nodes.erase(vfs_path);
}

void PlannerContext::clearContext(){
    visible_nodes.clear();
}

// ====== DiscussSession implementation ======
void DiscussSession::clear(){
    session_id.clear();
    conversation_history.clear();
    current_plan_path.clear();
    mode = Mode::Simple;
}

void DiscussSession::add_message(const std::string& role, const std::string& content){
    conversation_history.push_back(role + ": " + content);
}

std::string DiscussSession::generate_session_id(){
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(8) << dis(gen);
    return oss.str();
}

