# Tasks Tracker
Note: sexp is for AI and shell script is for human user (+ ai called via sexp). This follows the classic codex behaviour.


## Upcoming: important
- rename Stage1 to something else (remember to keep the U++ project file in .upp same)
- don't write all code in same h and cpp file. split code cleanly to multiple files
- add c++ ast functions to sexp (ai script) and ask AI to use them to create hello world
- vfs as persistent file. have single and/or multiple files, which encodes the content of the data in a way, that git commits work well (diffs or binary diffs?), and is size efficient too
	- autoload, if the app is ran in a directory with a default name file. (default name is yet to be decided; maybe like title of the directory + ".cx" ext)
	- autosave, backup save, restore from crash,
- overlays. support multiple persistent vfs files simultaneously in same vfs, without mixing files. when multiple overlays matches a path where you "cd", you virtually are in all of them.
	- conflicting overlays and files needs advanced policy handling; basically it shouldn't happen too often if we work via this program and don't copy-paste directories and files
- mount actual filesystems to directories (always as overlays though); enables actual file system reading/writing/browsing better; supports remote filesystems over network (see netcat later)
	- mount .dll or .so files to vfs' "/dev/" sub-directies: e.g. you have graphics windows, which can be written with raw data functions
- parse (libclang):
- make

## Upcoming: less important
- sh compatibility, login shell (commandline -l), evaluate first if some additional flags are needed
- turing complete script (like bash, csh). Let's talk about syntax and grammar before implementing. I do want to have intuitive scripting and more like tcsh
- netcat -like client and server. Also like tty server for CLI. Have room for minimal SSH later
	- also oneliner for remote computer usage; one line would some "remote" command or something; advanced remote shell integration
	- also filetransfer like scp (but like nc = without additional layers (encryption etc.))
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
- `AGENTS.md` drafted from discussion notes to document Stage1 agent scope.
- Build tooling pipeline now operational:
  - Root `Makefile` builds the Stage1 binary and exposes debug/release toggles.
  - OpenAI integration loads the API key from the home directory fallback (`~/openai-key.txt`).
  - `make sample` exercises the C++ AST builder, exports generated code, compiles it, and checks the runtime output.
- Stage1 harness (`tools/test_harness.py`) runs `.sexp` specs end-to-end against configured LLM targets and validates results inside codex-mini.
- C++ AST shell surface now includes statements (`cpp.vardecl`, `cpp.expr`, `cpp.stmt`, `cpp.return`, `cpp.rangefor`) for structural codegen beyond canned print/return helpers.

## Backlog / Ideas
- Harden string escaping in the C++ AST dumper before expanding code generation.
