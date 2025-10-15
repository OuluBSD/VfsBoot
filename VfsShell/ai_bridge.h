#pragma once


//
// Hash utilities (BLAKE3)
//
std::string compute_file_hash(const std::string& filepath);
std::string compute_string_hash(const std::string& data);

//
// AI helpers (OpenAI + llama.cpp bridge)
//
std::string build_responses_payload(const std::string& model, const std::string& user_prompt);
std::string call_openai(const std::string& prompt);
std::string call_llama(const std::string& prompt);
std::string call_ai(const std::string& prompt);
