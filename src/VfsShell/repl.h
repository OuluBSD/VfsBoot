#pragma once


// Forward declarations
struct Env;

// REPL function
void run_repl(Vfs& vfs, std::shared_ptr<Env> env);
