# U++ Convention Conversion Status

## Summary

Converting VfsBoot codebase from std:: types to Ultimate++ (U++) conventions following ai-upp fork standards.

**Current Status:** 128 compilation errors remaining (down from ~200+)

## Completed Conversions ✅

### Type System
- ✅ `std::shared_ptr<AstNode>` → `Shared<AstNode>` (ai-upp's Shared.h)
- ✅ `std::shared_ptr<Env>` → `Shared<Env>`
- ✅ Fixed `SharedPtr` → `Shared` (was incorrect casing)
- ✅ `SexpValue` inherits from `Moveable<SexpValue>` for U++ containers

### Method Signatures
- ✅ All `eval(std::shared_ptr<Env>)` → `eval(Shared<Env>)` across:
  - Sexp.h/cpp
  - CppAst.h
  - ClangParser.h
  - Planner.h

### String Conversions
- ✅ VfsMount.cpp - All String↔std::string conversions with `.ToStd()`
- ✅ Planner.cpp - `read()` and `write()` return/accept `String` types
- ✅ Feedback.cpp - `vfs.read()` calls with proper overlay parameters
- ✅ hash_hex() - `std::string` return → `String` return

### Container Methods
- ✅ Index methods: `insert()` → `FindAdd()`, `erase()` → `RemoveKey()`, `clear()` → `Clear()`

### VFS System
- ✅ Added mount system types (`MountType`, `MountInfo`) to VfsCommon.h
- ✅ Added mount-related members (`mount_allowed`, `mounts`) to Vfs struct
- ✅ Added mount method declarations to Vfs

### Utilities
- ✅ ClangParser.cpp - Added local `splitPath()` helper function
- ✅ path_basename/path_dirname declarations added to VfsCommon.h

### Documentation
- ✅ AGENTS.md - Added U++ Convention Adherence section
- ✅ QWEN.md - Added Coding Conventions (U++) section

## Remaining Work (128 errors)

### Top Priority Issues

#### 1. Missing Global Declarations (8 errors)
- **G_VFS** - Needs `extern Vfs* G_VFS;` declaration
- **g_on_save_shortcut** - Needs proper declaration (currently in ClangParser.cpp)

#### 2. String Method Issues (3 errors)
- **String.empty()** - U++ String doesn't have `.empty()`, use `.IsEmpty()` or check length
- Need to find all `.empty()` calls on String types and convert

#### 3. std::ostream Ambiguity (6 errors)
- Operator `<<` with String types causes ambiguity
- Solution: Use `.ToStd()` before streaming or use U++ streaming methods

#### 4. LogicFormula Type Conversions (4 errors)
- `std::vector<std::shared_ptr<LogicFormula>>` → `Vector<One<LogicFormula>>`
- `std::shared_ptr<LogicFormula>` → `One<LogicFormula>` or `Shared<LogicFormula>`
- Files: Logic/LogicEngine.cpp, Logic/TagSystem.cpp

#### 5. SVN/BinaryDiff Issues (4+ errors)
- ScopeStore.cpp has SVN delta types that don't exist
- **SvnDeltaContext**, **svn_string_t**, **svn_txdelta_stream_t**, etc.
- May need to disable this feature or use proper libsvn includes

#### 6. Container Type Mismatches
- Various `std::vector<>` → `Vector<>` needed
- `std::unordered_map<>` → container alternatives

#### 7. serialize_ast_node Issues (6 errors)
- Function signature mismatches between One<> and std::shared_ptr<>
- ClangParser.cpp serialization code needs conversion

## Next Steps

### Phase 1: Quick Wins (Reduce to ~100 errors)
1. Add G_VFS extern declaration
2. Fix String.empty() → String.IsEmpty()
3. Add g_on_save_shortcut declaration
4. Fix std::ostream << String with .ToStd()

### Phase 2: Container Conversions (Reduce to ~50 errors)
1. LogicFormula std::shared_ptr → One/Shared
2. Convert remaining std::vector → Vector where needed
3. Fix serialize/deserialize functions for One<> types

### Phase 3: Complex Issues
1. SVN/BinaryDiff - May need to disable or add proper dependencies
2. Remaining ClangParser.cpp conversions
3. Command.cpp, Registry.cpp iterations over DirListing

### Phase 4: Final Cleanup
1. Run full build
2. Fix any remaining edge cases
3. Test basic functionality

## U++ Type Reference

| std:: Type | U++ Type | Notes |
|------------|----------|-------|
| `std::string` | `String` | Use `.ToStd()` to convert, `.IsEmpty()` instead of `.empty()` |
| `std::vector<T>` | `Vector<T>` | Use `<<` instead of `push_back()`, `Add()` also available |
| `std::map<K,V>` | `VectorMap<K,V>` or `ArrayMap<K,V>` | Ordered containers |
| `std::unordered_map<K,V>` | `Index<K,V>` | Use `FindAdd()`, `Find()`, `RemoveKey()` |
| `std::shared_ptr<T>` | `Shared<T>` or `One<T>` | `Shared` for ref-counted, `One` for owned |
| `T*` (raw, non-owning) | `Ptr<T>` or `Pte<T>` | Validity-checking pointers |

## Build Commands

```bash
# UMK build (current)
./umk_build_VfsShell.sh

# Count errors
./umk_build_VfsShell.sh 2>&1 | grep -c "error:"

# View most common errors
./umk_build_VfsShell.sh 2>&1 | grep "error:" | cut -d: -f3- | sort | uniq -c | sort -rn | head -20
```

---

**Last Updated:** Current session
**Errors Remaining:** 128
**Files Modified:** 15+
**Conversion Progress:** ~40%
