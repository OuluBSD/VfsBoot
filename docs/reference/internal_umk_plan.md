# Internal U++ Builder Plan

## Context
- Current `upp.wksp.build` shells out to `umk` through builder `COMMAND`.
- We want a self-contained path that compiles and links inside VfsShell, while still allowing custom builders.
- Builder metadata (e.g. `COMPILER`, `COMMON_OPTIONS`, `DEBUG_OPTIONS`) already loads into `UppBuildMethod`; we can reinterpret it to drive the internal pipeline.

## Proposed Architecture
1. **Toolchain Extraction**
   - Introduce `UppToolchain` (struct/class) that normalizes compiler, linker, include/lib dirs, and flag bundles for debug/release.
   - Resolve workspace-relative paths to host paths via `Vfs::mapToHostPath`.
   - Expose helpers: `effectiveCompileFlags(TunitKind, BuildType)`, `effectiveLinkFlags(BuildType)`, `discoverSources(pkg)`.

2. **Graph Modelling**
   - Extend `BuildCommand::Type` with `UppCompile` / `UppLink`.
   - Provide executors that invoke `compiler`/`linker` directly (via `popen` or `std::system`) but driven by structured metadata (source path, output obj, flags).
   - Generate per-source compilation nodes plus a link node per package; preserve dependency ordering (`pkg -> libs`).

3. **Integration & Fallback**
   - When builder lacks `COMMAND`, synthesize the internal plan:
     - Determine output directory (respect `--output` or default `base/out/pkg`).
     - Emit compile/link commands with absolute host paths.
     - Optionally support dry-run by printing the planned steps.
   - Still honor custom `COMMAND` if present (AI/remote/backends continue to work).

4. **Testing/Validation**
   - Add scripted scenario under `scripts/unittst/` to compile a trivial package using the new pipeline.
   - Ensure we can mix host and VFS files, and that error handling percolates through `BuildResult`.

## Open Questions
- C caching/BLITZ: initial cut can skip; document as future work.
- Multi-language support (C, C++, assembly) requires source-kind detection (maybe from file suffix).
- Should we leverage `clang` tooling already bundled (e.g. include auto-detected flags).

## Next Steps
1. Scaffold `UppToolchain` + config parsing.
2. Extend build graph executor to handle structured commands.
3. Wire default pipeline into `upp_workspace_build.cpp` when no explicit `COMMAND`.
4. Add integration test script and documentation updates.

