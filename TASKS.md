# Tasks Tracker
Note: sexp is for AI and shell script is for human user (+ ai called via sexp). This follows the classic codex behaviour.


## Upcoming: important
- add shell commands ctrl+u and ctrl+k for clearing text
- vfs as persistent file. have single and/or multiple files, which encodes the content of the data in a way, that git commits work well (diffs or binary diffs?), and is size efficient too
	- autoload, if the app is ran in a directory with a default name file. (default name is yet to be decided; maybe like title of the directory + ".cx" ext)
	- autosave, backup save, restore from crash,
- mount actual filesystems to directories (always as overlays though); enables actual file system reading/writing/browsing better; supports remote filesystems over network (see netcat later)
	- mount .dll or .so files to vfs' "/dev/" sub-directies: e.g. you have graphics windows, which can be written with raw data functions
- planner core engine for breakdown/context (action/state model, A* search, cost heuristics)
- scope store with binary diffs + feature masks, plus deterministic context builder
- scenario harness binaries (`planner_demo`, `planner_train`) and scripted breakdown loop for validation
- feedback pipeline for planner rule evolution (metrics capture, rule patch staging, optional AI assistance)
- integrate planner/context system into CLI once core pieces are stable
- add in-binary sample runner command `sample.run`
	- register `sample.run` in the shell command dispatcher so demos/tests can call it directly
	- reset `/astcpp/demo`, `/cpp/demo.cpp`, and `/logs/sample.*` before each run for deterministic state
	- construct the demo translation unit via C++ AST helpers and mirror the existing "Hello" program steps internally
	- dump the generated source back into `/cpp/demo.cpp` to keep user export workflows intact
	- locate the host compiler (from `/env/compiler`, env var, or default `c++`) and compile to a temporary executable
	- capture compiler stdout/stderr into VFS logs (`/logs/sample.compile.out`, `/logs/sample.compile.err`)
	- execute the compiled binary, recording output into `/logs/sample.run.out`, `/logs/sample.run.err`
	- write a status node (e.g. `/env/sample.status`) summarizing success/failure, exit codes, and timings
	- propagate failure by returning non-zero exit codes when compilation or execution fails
	- accept optional flags such as `--keep` or `--trace` for temp retention and verbose diagnostics
	- update documentation and scripts to reference `sample.run`, replacing the Makefile's external pipeline
	- extend automated tests to invoke `sample.run` and validate status/output log contents
- rename Stage1 to something else (remember to keep the U++ project file in .upp same)
- don't write all code in same h and cpp file. split code cleanly to multiple files
- parse (libclang):
- make

## Upcoming: less important
- sh compatibility, login shell (commandline -l), evaluate first if some additional flags are needed
- turing complete script (like bash, csh). Let's talk about syntax and grammar before implementing. I do want to have intuitive scripting and more like tcsh
- netcat -like client and server. Also like tty server for CLI. Have room for minimal SSH later
	- also oneliner for remote computer usage; one line would some "remote" command or something; advanced remote shell integration
	- also filetransfer like scp (but like nc = without additional layers (encryption etc.))
- some internal logs visible in /log (vfs)
- CLI autocomplete with tab
- ncurses (+ windows etc alternative) minimal text editor
- resolver for cpp ast nodes
	- keeping the codebase compatible with non-raii, dynamic memory typed languages, like java & C#
- add support for java & c# & typescript & javascript & python
- alias for script functions; e.g. cpp.returni could be like "cri"

- sexp to javascript to sexp converter
	- also for python, powershell, bash, etc.
	
## Upcoming: less important or skip altogether
- byte-vm (maybe overkill?) for sexp, or cx script

## Upcoming: important tests
- we should have small real-life examples, where we create "int main()". Then we need something new there couple of times: user -> system -> user -> system interaction; and the system searches and modifies the ast tree.
	- you should plan like 5 progressively more difficult interaction demos
- we should have actual programming project in a directory, with multiple files, which is mounted to the vfs as overlay. the code is kept both in persistent vfs-file and as cpp/h/Makefile files

## 

## Completed
- AI bridge prompt & tests: added scripts/examples/ai-hello-world.sexp and tests/011-ai-bridge-hello.sexp to exercise cpp.* helpers via the ai command.
- `AGENTS.md` drafted from discussion notes to document Stage1 agent scope.
- Build tooling pipeline now operational:
  - Root `Makefile` builds the Stage1 binary and exposes debug/release toggles.
  - OpenAI integration loads the API key from the home directory fallback (`~/openai-key.txt`).
  - `make sample` exercises the C++ AST builder, exports generated code, compiles it, and checks the runtime output.
- Stage1 harness (`tools/test_harness.py`) runs `.sexp` specs end-to-end against configured LLM targets and validates results inside codex-mini.
- C++ AST shell surface now includes statements (`cpp.vardecl`, `cpp.expr`, `cpp.stmt`, `cpp.return`, `cpp.rangefor`) for structural codegen beyond canned print/return helpers.
- overlays: multiple persistent VFS overlays can now coexist without mixing nodes; the CLI exposes `overlay.*` commands and aggregate listings.

## Backlog / Ideas
- Harden string escaping in the C++ AST dumper before expanding code generation.
