# Assembly Artifacts

This directory holds packaged overlays (`.cxpkg`) and raw assemblies (`.cxasm`) that are awkward to keep under `scripts/`.  Files here are ready-to-mount examples that demonstrate cross-session persistence features in VfsShell.

- `ast-roundtrip.cxpkg` — minimal VFS snapshot exercising the new AST serializer. It contains a parsed S-expression stored under `/ast/expr` and a small C++ translation unit at `/astcpp/demo`. Mount it via `overlay.mount demo assembly/ast-roundtrip.cxpkg` and explore with `tree /`.
- `ast-roundtrip-copy.cxpkg` — same snapshot re-saved after a reload, leaving both the original and a duplicated AST under `/ast`. It is handy when you want to confirm that re-parsing into an existing overlay still stores AST nodes correctly.
