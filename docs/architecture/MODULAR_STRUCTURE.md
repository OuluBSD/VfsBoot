# VfsBoot Modular File Structure Analysis

## Executive Summary

The VfsBoot codebase currently consists of two monolithic files:
- **VfsShell/codex.h**: 2,335 lines (header file)
- **VfsShell/codex.cpp**: 11,526 lines (implementation)

This analysis proposes splitting these into **20 focused modules** organized in 9 dependency levels, resulting in files averaging 200-800 lines each.

## Current Structure Analysis

### codex.h Structure (2,335 lines)
| Lines | Component |
|-------|-----------|
| 1-97 | Standard includes, tracing, i18n |
| 98-422 | Tag System (BitVector, TagSet, TagRegistry, TagStorage) |
| 424-506 | Logic Engine (LogicFormula, ImplicationRule, LogicEngine) |
| 507-593 | VFS Core Nodes (VfsNode, DirNode, FileNode, Mount types) |
| 594-671 | S-expression System (Value, Env, AstNode hierarchy) |
| 673-686 | Tag Storage |
| 687-812 | Vfs class (main filesystem with overlays) |
| 814-839 | VfsVisitor |
| 840-851 | S-expression Parser |
| 852-963 | C++ AST Builder nodes |
| 974-1394 | libclang AST nodes |
| 1395-1400 | libclang Parser |
| 1401-1502 | Planner nodes (PlanNode hierarchy) |
| 1503-1523 | DiscussSession |
| 1524-1537 | Hash & AI helpers |
| 1538-1681 | Context Builder |
| 1682-1714 | ReplacementStrategy |
| 1715-1853 | Hypothesis Testing |
| 1855-2040 | Scope Store (binary diffs, snapshots) |
| 2041-2251 | Feedback Pipeline |
| 2252-2311 | GNU Make implementation |
| 2312-2335 | Web Server |

### codex.cpp Structure (11,526 lines)
| Lines | Component |
|-------|-----------|
| 1-75 | Includes |
| 76-162 | i18n implementation |
| 163-568 | BLAKE3 hash, line editing, tab completion |
| 569-1031 | libclang parser implementation |
| 1032-1159 | S-expression AST serialization |
| 1160-1832 | C++ AST serialization & helpers |
| 1833-2605 | **Shell command implementations (majority!)** |
| 2606-2670 | AST node constructors & eval |
| 2671-3176 | VFS core implementation |
| 3177-3532 | Mount nodes implementation |
| 3533-3634 | Tag Registry & Storage |
| 3635-4246 | Logic System implementation |
| 4247-4344 | Tag mining & VFS tag helpers |
| 4345-4428 | S-expression Parser |
| 4429-4571 | Builtins |
| 4572-4786 | C++ AST node implementations |
| 4787-4995 | Planner nodes |
| 4996-5027 | PlannerContext |
| 5028-5049 | DiscussSession |
| 5050-5458 | OpenAI/Llama AI helpers |
| 5459-5910 | Context Builder |
| 5911-6067 | ReplacementStrategy |
| 6068-6126 | ActionPlannerTest |
| 6127-6667 | Hypothesis Testing |
| 6668-7166 | Scope Store |
| 7167-7799 | Feedback Pipeline |
| 7800-7942 | REPL utilities |
| 7943-8267 | GNU Make |
| 8268-11526 | **Main function & shell command dispatch** |

## Proposed Modular Structure

### Module Breakdown (20 modules)

#### Level 0: Foundation
1. **vfs_common** (base layer)
   - Standard includes, tracing, i18n, hash utilities
   - ~200 lines header, ~250 lines cpp

#### Level 1: Data Structures
2. **tag_system**
   - TagId, BitVector, TagSet, TagRegistry, TagStorage, TagMiningSession
   - ~450 lines header, ~250 lines cpp
   - Depends on: vfs_common

#### Level 2: Logic & Core
3. **logic_engine**
   - LogicFormula, ImplicationRule, LogicEngine
   - ~250 lines header, ~650 lines cpp
   - Depends on: tag_system

4. **vfs_core**
   - VfsNode, DirNode, FileNode, Vfs, VfsVisitor
   - ~400 lines header, ~700 lines cpp
   - Depends on: tag_system, logic_engine

#### Level 3: Extensions
5. **vfs_mount**
   - MountNode, LibraryNode, RemoteNode
   - ~200 lines header, ~500 lines cpp
   - Depends on: vfs_core

6. **sexp** (S-expression system)
   - Value, Env, AstNode hierarchy, parser, builtins
   - ~250 lines header, ~500 lines cpp
   - Depends on: vfs_core

7. **ai_bridge**
   - OpenAI/Llama integration
   - ~50 lines header, ~450 lines cpp
   - Depends on: vfs_common

#### Level 4: AST Systems
8. **cpp_ast** (C++ AST Builder)
   - CppNode hierarchy, code generation
   - ~250 lines header, ~500 lines cpp
   - Depends on: sexp

9. **clang_parser** (libclang integration)
   - ClangAstNode hierarchy, parser
   - ~550 lines header, ~600 lines cpp
   - Depends on: sexp, vfs_core

10. **planner** (Planning system)
    - PlanNode hierarchy, PlannerContext, DiscussSession
    - ~250 lines header, ~350 lines cpp
    - Depends on: sexp, vfs_core

#### Level 5: Advanced Features
11. **context_builder** (AI context management)
    - ContextFilter, ContextEntry, ContextBuilder, ReplacementStrategy
    - ~300 lines header, ~650 lines cpp
    - Depends on: tag_system, vfs_core

12. **make** (GNU Make subset)
    - MakeRule, MakeFile parser/executor
    - ~100 lines header, ~400 lines cpp
    - Depends on: vfs_core

#### Level 6: Analysis & Testing
13. **hypothesis** (Hypothesis testing)
    - Hypothesis, HypothesisTester, ActionPlannerTest
    - ~250 lines header, ~700 lines cpp
    - Depends on: context_builder

14. **scope_store** (Binary diffs & snapshots)
    - BinaryDiff, FeatureMask, ScopeSnapshot, ScopeStore
    - ~350 lines header, ~650 lines cpp
    - Depends on: vfs_core, context_builder

15. **feedback** (Feedback pipeline)
    - PlannerMetrics, RulePatch, FeedbackLoop
    - ~350 lines header, ~800 lines cpp
    - Depends on: logic_engine, vfs_core

#### Level 7: Interface
16. **web_server** (HTTP/WebSocket server)
    - WebServer namespace functions
    - ~50 lines header, ~TBD lines cpp
    - Depends on: vfs_common

17. **shell_commands** (Shell command implementations)
    - All cmd_* functions, command dispatch
    - ~100 lines header, ~3000+ lines cpp
    - Depends on: (most modules - orchestration layer)

#### Level 8: Top Layer
18. **repl** (REPL loop)
    - Main REPL loop, line editing, tab completion
    - ~50 lines header, ~400 lines cpp
    - Depends on: shell_commands

19. **main** (Entry point)
    - main() function, initialization
    - ~300 lines cpp
    - Depends on: repl, web_server

20. **ui_backend** (existing)
    - UIBackend abstraction (already separate)

## Dependency Graph

```
                                    main.cpp
                                       |
                         +-------------+-------------+
                         |                           |
                      repl                      web_server
                         |                           |
                  shell_commands                 vfs_common
                         |
         +---------------+----------------+
         |               |                |
    hypothesis      scope_store       feedback
         |               |                |
         |          vfs_core         logic_engine
         |               |                |
   context_builder  +----+----+      tag_system
         |           |         |           |
    tag_system   vfs_mount   sexp      vfs_common
         |           |         |
    vfs_common   vfs_core  vfs_core
                     |         |
              logic_engine  (etc.)
                     |
               tag_system
                     |
                vfs_common


Detailed Bottom-Up:
Level 0: vfs_common, ui_backend
Level 1: tag_system
Level 2: logic_engine, vfs_core
Level 3: vfs_mount, sexp, ai_bridge
Level 4: cpp_ast, clang_parser, planner
Level 5: context_builder, make
Level 6: hypothesis, scope_store, feedback
Level 7: web_server, shell_commands
Level 8: repl, main
```

## Key Components by Module

### 1. vfs_common (Foundation)
**Header:**
- Standard includes (algorithm, vector, map, memory, etc.)
- Tracing macros: TRACE_FN, TRACE_MSG, TRACE_LOOP
- i18n: MsgId enum, get(), init(), set_english_only()
- Forward declarations: struct Vfs

**Implementation:**
- codex_trace::log_line(), Scope ctor/dtor
- i18n implementation
- compute_file_hash(), compute_string_hash() (BLAKE3)

### 2. tag_system
**Header:**
- TagId typedef
- BitVector class (64-bit chunk-based bit vector)
- TagSet class (set operations on TagIds)
- TagRegistry (name↔id mapping)
- TagStorage (VfsNode*→TagSet mapping)
- TagMiningSession

**Implementation:**
- TagRegistry::registerTag(), getTagId(), getTagName()
- TagStorage::addTag(), removeTag(), hasTag(), findByTag()
- TagMiningSession::addUserTag(), recordFeedback()

### 3. logic_engine
**Header:**
- LogicOp enum (VAR, NOT, AND, OR, IMPLIES)
- LogicFormula (theorem proving formulas)
- ImplicationRule (premise → conclusion)
- LogicEngine (forward chaining, SAT, consistency)

**Implementation:**
- LogicFormula factory methods (makeVar, makeNot, makeAnd, etc.)
- LogicFormula::evaluate(), toString()
- LogicEngine::addRule(), inferTags(), checkConsistency()
- Rule serialization/deserialization
- Hard-coded domain rules

### 4. vfs_core
**Header:**
- VfsNode base class (name, parent, kind)
- DirNode, FileNode
- Vfs class:
  - Overlay system
  - Path resolution (resolve, resolveMulti)
  - Directory operations (mkdir, ls, tree)
  - File operations (touch, write, read, rm, mv, link)
  - Tag helpers (addTag, removeTag, etc.)
- VfsVisitor (find operations)

**Implementation:**
- Vfs::splitPath()
- Overlay management (registerOverlay, overlayRoot, etc.)
- Path resolution logic
- Directory listing with overlay merging
- Tree visualization (basic and advanced)
- Tag management delegation to TagStorage

### 5. vfs_mount
**Header:**
- MountNode (filesystem mounts)
- LibraryNode (shared library .so mounts)
- LibrarySymbolNode
- RemoteNode (remote VFS over TCP)
- Vfs mount management methods

**Implementation:**
- MountNode: populateCache(), isDir(), read(), write()
- LibraryNode: dlopen/dlsym integration
- RemoteNode: TCP connection, remote command execution
- Vfs::mountFilesystem(), mountLibrary(), mountRemote()
- Vfs::unmount(), listMounts()

### 6. sexp (S-expression System)
**Header:**
- Value (variant: int64_t, bool, string, Builtin, Closure, List)
- Env (environment with parent chain)
- AstNode base class
- AstInt, AstBool, AstStr, AstSym, AstIf, AstLambda, AstCall, AstHolder
- Token, lex(), parse()
- install_builtins()

**Implementation:**
- Value::show()
- Value factory methods (I, B, S, Built, Clo, L)
- AstNode constructors
- AstNode::eval() for each type
- Lexer (lex)
- Parser (parse)
- Builtins (+, -, *, /, ==, if, lambda, list, car, cdr, etc.)

### 7. ai_bridge
**Header:**
- build_responses_payload()
- call_openai()
- call_llama()
- call_ai() (dispatch to OpenAI or Llama)

**Implementation:**
- HTTP POST to OpenAI API
- HTTP POST to Llama server
- JSON payload construction
- Response parsing
- Environment variable handling (OPENAI_API_KEY, LLAMA_BASE_URL, etc.)

### 8. cpp_ast (C++ AST Builder)
**Header:**
- CppNode base class (inherits AstNode)
- CppInclude
- CppExpr hierarchy (CppId, CppString, CppInt, CppCall, CppBinOp, CppStreamOut, CppRawExpr)
- CppStmt hierarchy (CppExprStmt, CppReturn, CppRawStmt, CppVarDecl, CppCompound, CppRangeFor)
- CppFunction, CppTranslationUnit
- Helper functions: expect_tu, expect_fn, expect_block, vfs_add, cpp_dump_to_vfs

**Implementation:**
- CppNode::dump() for each type
- String escaping (CppString::esc)
- Code generation logic
- VFS integration helpers

### 9. clang_parser (libclang Integration)
**Header:**
- SourceLocation
- ClangAstNode base class
- ClangType hierarchy (ClangBuiltinType, ClangPointerType, ClangReferenceType, etc.)
- ClangDecl hierarchy (ClangFunctionDecl, ClangVarDecl, ClangClassDecl, etc.)
- ClangStmt hierarchy (ClangIfStmt, ClangForStmt, ClangWhileStmt, etc.)
- ClangExpr hierarchy (ClangBinaryOperator, ClangCallExpr, ClangDeclRefExpr, etc.)
- ClangPreprocessor nodes (ClangMacroDefinition, ClangInclusionDirective)
- ClangParser class
- Shell commands: cmd_parse_file, cmd_parse_dump, cmd_parse_generate

**Implementation:**
- ClangParser::parseFile(), parseString()
- ClangParser::convertCursor() (dispatch to handleDeclaration/Statement/Expression)
- ClangAstNode::dump() for each type
- SourceLocation extraction
- Type string conversion
- Shell command implementations for parsing

### 10. planner
**Header:**
- PlanNode base class (inherits AstNode)
- PlanRoot, PlanSubPlan, PlanGoals, PlanIdeas, PlanStrategy, PlanJobs, PlanDeps, PlanImplemented, PlanResearch, PlanNotes
- PlanJob struct
- PlannerContext (navigation state, forward/backward, context visibility)
- DiscussSession (conversation state)

**Implementation:**
- PlanNode::read()/write() for each type
- PlanJobs::addJob(), completeJob(), getSortedJobIndices()
- PlannerContext::navigateTo(), forward(), backward()
- DiscussSession::clear(), add_message(), generate_session_id()

### 11. context_builder
**Header:**
- ContextFilter (TagAny, TagAll, PathPrefix, ContentMatch, etc.)
- ContextEntry (VFS node + metadata + token estimate)
- ContextBuilder (collect, filter, build within token budget)
- ContextBuilder::ContextOptions (dependencies, dedup, summary, hierarchical)
- ReplacementStrategy (code modification strategies)

**Implementation:**
- ContextFilter factory methods
- ContextFilter::matches()
- ContextEntry::estimate_tokens()
- ContextBuilder::collect(), build(), buildWithPriority()
- ContextBuilder::buildWithOptions() (advanced features)
- ContextBuilder::deduplicateEntries()
- ReplacementStrategy::apply()

### 12. make (GNU Make Subset)
**Header:**
- MakeRule (target, dependencies, commands)
- MakeFile (parser, executor)

**Implementation:**
- MakeFile::parse() (Makefile syntax)
- Variable substitution ($(VAR), ${VAR})
- Automatic variables ($@, $<, $^)
- MakeFile::needsRebuild() (timestamp comparison)
- MakeFile::build() (dependency graph, cycle detection)
- MakeFile::executeCommands() (shell command execution)

### 13. hypothesis (Hypothesis Testing)
**Header:**
- Hypothesis (5 levels: SimpleQuery, CodeModification, Refactoring, FeatureAddition, Architecture)
- HypothesisResult (findings, actions, diagnostics)
- HypothesisTester (validation without AI)
- HypothesisTestSuite
- ActionPlannerTest, ActionPlannerTestSuite

**Implementation:**
- HypothesisTester::testSimpleQuery() (Level 1)
- HypothesisTester::testErrorHandlingAddition() (Level 2)
- HypothesisTester::testDuplicateExtraction() (Level 3)
- HypothesisTester::testLoggingInstrumentation() (Level 4)
- HypothesisTester::testArchitecturePattern() (Level 5)
- Helper methods: findFunctionDefinitions, findDuplicateBlocks, etc.
- HypothesisTestSuite::runAll(), printResults()

### 14. scope_store (Binary Diffs & Snapshots)
**Header:**
- BinaryDiff (SVN delta algorithm)
- FeatureMask (512-bit feature vector)
- ScopeSnapshot (VFS state + diff from parent)
- ScopeStore (snapshot management)
- DeterministicContextBuilder

**Implementation:**
- BinaryDiff::compute() (SVN delta encoding)
- BinaryDiff::apply() (reconstruct from diff)
- FeatureMask serialization
- ScopeStore::createSnapshot(), restoreSnapshot()
- ScopeStore::save(), load()
- DeterministicContextBuilder::build() (reproducible context)

### 15. feedback (Feedback Pipeline)
**Header:**
- PlannerMetrics (scenario results, rule triggers)
- MetricsCollector (capture execution data)
- RulePatch (Add, Modify, Remove, AdjustConfidence)
- RulePatchStaging (pending/applied/rejected)
- FeedbackLoop (analyze metrics, generate patches)

**Implementation:**
- MetricsCollector::startRun(), recordRuleTrigger(), finishRun()
- RulePatch factory methods, serialization
- RulePatchStaging::stagePatch(), applyPatch(), rejectPatch()
- FeedbackLoop::analyzeMetrics(), runCycle()
- Pattern detection, confidence adjustment

### 16. web_server
**Header:**
- WebServer namespace
- start(), stop(), is_running(), send_output(), set_command_callback()

**Implementation:**
- libwebsockets integration
- HTTP server for terminal UI
- WebSocket for real-time communication
- xterm.js frontend (HTML/JS embedded)

### 17. shell_commands
**Header:**
- Command dispatch function
- Tab completion helpers
- get_all_command_names(), get_vfs_path_completions()

**Implementation:**
- **All cmd_* functions** (~50+ commands):
  - VFS: ls, tree, tree.adv, mkdir, touch, cat, echo, rm, mv, link
  - S-expr: parse, eval
  - C++ AST: cpp.tu, cpp.include, cpp.func, cpp.param, cpp.expr, etc.
  - libclang: parse.file, parse.dump, parse.generate
  - Planner: plan.create, plan.goto, plan.forward, plan.discuss, etc.
  - AI: ai, tools, discuss
  - Context: context.build, context.filter.tag, context.filter.path
  - Hypothesis: hypothesis.query, hypothesis.errorhandling, etc.
  - Feedback: feedback.metrics.show, feedback.patches.apply, feedback.cycle
  - Logic: logic.init, logic.infer, logic.check, logic.explain
  - Make: make
  - Overlay: overlay.mount, overlay.save, overlay.list, etc.
  - Mount: mount.fs, mount.lib, mount.remote, unmount
  - Tags: tag.add, tag.remove, tag.list, tag.find
  - Misc: help, exit, sample.run
- Command dispatch logic
- Tab completion implementation

### 18. repl
**Header:**
- run_repl() (main REPL loop)
- Line editing utilities

**Implementation:**
- REPL loop (read-eval-print)
- Line editing with history
- Tab completion integration
- Signal handling
- Error handling

### 19. main
**Implementation only:**
- main() function
- Command-line argument parsing (--web-server, --port, --english-only)
- VFS initialization
- Overlay loading
- Builtin installation
- i18n initialization
- Mode selection (REPL vs web server vs daemon)

### 20. ui_backend (existing)
**Header:**
- UIBackend abstract class
- run_ncurses_editor() function

## Migration Strategy

### Phase 1: Foundation (Week 1)
1. Create `vfs_common.h` and `vfs_common.cpp`
   - Extract includes, tracing, i18n, hash utilities
2. Create `tag_system.h` and `tag_system.cpp`
   - Extract TagId, BitVector, TagSet, TagRegistry, TagStorage
3. Create `logic_engine.h` and `logic_engine.cpp`
   - Extract LogicFormula, ImplicationRule, LogicEngine
4. Update `codex.h` to `#include` these headers
5. Test compilation

### Phase 2: Core VFS (Week 2)
1. Create `vfs_core.h` and `vfs_core.cpp`
   - Extract VfsNode, DirNode, FileNode, Vfs, VfsVisitor
2. Create `vfs_mount.h` and `vfs_mount.cpp`
   - Extract MountNode, LibraryNode, RemoteNode
3. Test compilation

### Phase 3: AST Systems (Week 3)
1. Create `sexp.h` and `sexp.cpp`
   - Extract Value, Env, AstNode hierarchy, parser, builtins
2. Create `cpp_ast.h` and `cpp_ast.cpp`
   - Extract CppNode hierarchy
3. Create `clang_parser.h` and `clang_parser.cpp`
   - Extract ClangAstNode hierarchy, ClangParser
4. Create `planner.h` and `planner.cpp`
   - Extract PlanNode hierarchy, PlannerContext, DiscussSession
5. Test compilation

### Phase 4: Advanced Features (Week 4)
1. Create `ai_bridge.h` and `ai_bridge.cpp`
2. Create `context_builder.h` and `context_builder.cpp`
3. Create `hypothesis.h` and `hypothesis.cpp`
4. Create `scope_store.h` and `scope_store.cpp`
5. Create `feedback.h` and `feedback.cpp`
6. Create `make.h` and `make.cpp`
7. Test compilation

### Phase 5: Interface Layer (Week 5)
1. Create `web_server.h` and `web_server.cpp`
2. Create `shell_commands.h` and `shell_commands.cpp`
   - Extract all cmd_* functions
3. Create `repl.h` and `repl.cpp`
4. Create `main.cpp`
5. Remove `codex.h` and `codex.cpp`
6. Update Makefile to compile all new files
7. Full test suite run
8. Git commit

## Benefits

1. **Maintainability**
   - Each module ~200-800 lines (digestible)
   - Clear responsibility for each file
   - Easy to locate and modify specific functionality

2. **Compilation Speed**
   - Parallel compilation of independent modules
   - Incremental builds (only recompile changed modules)
   - Reduced header dependencies

3. **Clear Architecture**
   - DAG dependency structure (no cycles)
   - Bottom-up design clearly visible
   - Easy to understand system layers

4. **Testing**
   - Unit test each module independently
   - Mock dependencies easily
   - Integration testing by layer

5. **Collaboration**
   - Multiple developers work on different modules
   - Reduced merge conflicts
   - Clear ownership boundaries

6. **Documentation**
   - Each file documents one subsystem
   - Dependency graph shows relationships
   - Easier onboarding for new contributors

7. **Reusability**
   - Extract modules for other projects (e.g., tag_system, vfs_core)
   - Stand-alone components
   - Cleaner API boundaries

## Risks & Mitigation

### Risk 1: Breaking Existing Code
**Impact:** High
**Probability:** Medium
**Mitigation:**
- Keep all public APIs identical (just move, don't modify)
- Test compilation after each phase
- Run existing test suite after each phase
- Use git branches for incremental work

### Risk 2: Circular Dependencies
**Impact:** High
**Probability:** Low
**Mitigation:**
- Follow strict dependency graph (bottom-up)
- Use forward declarations where possible
- Break circular deps with interfaces if needed
- Static analysis to detect cycles

### Risk 3: Compilation Errors
**Impact:** Medium
**Probability:** Medium
**Mitigation:**
- Incremental approach (one module at a time)
- Compile and test after each extraction
- Keep detailed notes on move operations
- Pair programming for complex extractions

### Risk 4: Performance Regression
**Impact:** Low
**Probability:** Low
**Mitigation:**
- Headers use inline for small functions (as before)
- No runtime overhead (only compile-time structure)
- Profile before/after to verify
- Link-time optimization (LTO) if needed

### Risk 5: Increased Build Complexity
**Impact:** Low
**Probability:** Medium
**Mitigation:**
- Update Makefile carefully
- Use wildcards for .cpp files
- Keep build scripts simple
- Document build process

## Implementation Checklist

For each module extraction:
- [ ] Create header file with include guards
- [ ] Add dependencies (includes)
- [ ] Move class/struct declarations
- [ ] Move function declarations
- [ ] Create cpp file
- [ ] Include own header first
- [ ] Move implementations
- [ ] Update original codex.h to include new header
- [ ] Remove moved code from codex.cpp
- [ ] Add new files to Makefile
- [ ] Compile and fix errors
- [ ] Run test suite
- [ ] Git commit with descriptive message

## File Size Estimates

Total lines in proposed structure:
- Headers: ~4,750 lines (20 files, avg ~240 lines each)
- Implementation: ~12,600 lines (20 files, avg ~630 lines each)
- **Total: ~17,350 lines** (vs current 13,861 lines)

Size increase due to:
- Include guards in headers (~40 lines)
- Separation of declarations/implementations
- Better documentation in each file

## Conclusion

This modular structure provides:
- Clear separation of concerns
- Logical dependency hierarchy
- Improved maintainability
- Faster compilation
- Better testability
- Easier collaboration

Recommended approach: Incremental migration over 5 weeks, with compilation and testing after each phase.
