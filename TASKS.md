# Tasks Tracker
Note: sexp is for AI and shell script is for human user (+ ai called via sexp). This follows the classic codex behaviour.


## Upcoming: important (in order)
- rename Stage1 to VfsShell. update all related files
- don't write all code in same h and cpp file. split code cleanly to multiple files. write AGENTS.md in VfsShell directory to explain files. Link that to root AGENTS.md
- add examples of how to run all files in script dir. Add those to HOWTO_SCRIPTS.md file. Explain sexp files in script files and make cx scripts if you can't open them with single sh command
- when I run "python tools/test_harness.py --target llama -v" I see some fails even though they shouldn't. I think you should add c++ compiler and to compile and to run given programs to make them pass. We can't really pass or fail these responses based on AST. We can only pass them if they echo some expected message. So you need to change all tests made by test_harness to compare echoed text to expected
	- safe sandbox for running these executables
	- tests/003-string-view.sexp seems to hang for me and do something incorrect: it just prints tools
- vfs as persistent file. have single and/or multiple files, which encodes the content of the data in a way, that git commits work well (diffs or binary diffs?), and is size efficient too
	- autoload, if the app is ran in a directory with a default name file. (default name is yet to be decided; maybe like title of the directory + ".cx" ext)
	- autosave, backup save, restore from crash,
- mount actual filesystems to directories (always as overlays though); enables actual file system reading/writing/browsing better; supports remote filesystems over network (see netcat later)
	- mount .dll or .so files to vfs' "/dev/" sub-directies: e.g. you have graphics windows, which can be written with raw data functions
- planner core engine for breakdown/context (action/state model, A* search, cost heuristics)
	- we need a very hierarchical repository-wide plan with many vertical steps and details, to work even with low quality AI. we need multiple types of AI (generic, pair programming). let's discuss how we create a system to use low quality AI agents
		- we should take hints from Gentoo's emerge: USE flags and a list of "packages" to emerge. We should estimate the whole program starting from the most coarsest node and then add details
			- keep track if USE flag is written by user or is it agent's estimate
		- let's ask yes/no or explain questions from human expert about details of where to put code and features
	- vfs must have tags (and types etc) for basic filtering (and combining tags hierarchically). sections hidden in plain c++ should have tags (like #include section, sub-statement-list section). We should be able to create code comments (//) from tags. We could even use action planner to make comments using context (and tags)
- action planner test suite: what to add to context list. tag based filtering, "if vfs node contains x" based filtering, etc (huge list, figure it out). this is the AI context offloader. This is used also to figure out the list of statements to be removed before adding new code (==replacing). it must be last resort to dump actual c++ statements in c++ code as text for ai to figure it out. we must have a working "action planner hypothesis" from ai, or multiples (tree-like hypothesis), which we can test before calling AI again. the most difficult tests are "templates for modifying or replacing code" or "commenting code based on tags and context". We can use those comment generators to verify, that action planner works: ask action planner if this comment or attribute-list is valid.
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
- make
- parse (libclang): import clang test suite files to vfs
	- also collect what preprocessor sees
- gui app, which shows graph-based images of what AI does (static image slideshow), and other info too. it is also used for image related ai work

## Upcoming: less important
- commandline arguments: --llama, --openai, --version/-v, --help/-h, etc.
- explain different causes of sexp, cx, cxpkg files. discuss with me of them if you're unsure. write to README.md and AGENTS.md
	- make a solution with multiple cxasm & cxpkg packages, which all have multiple cpp and h files. compile and run it succesfully
- CLI home + end button usage while editing prompt
- when I press 'ä', it does nothing. fix it
- when I type "ai some message", I need to push enter twice instead of once
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
- test_harness.py now uses AI response caching compatible with C++ cache format (cache/ai/{provider}/{hash}-in.txt and {hash}-out.txt)
- add shell commands ctrl+u and ctrl+k for clearing text
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
