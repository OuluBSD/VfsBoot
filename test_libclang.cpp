#include <clang-c/Index.h>
#include <iostream>
#include <string>
#include <cstring>

int main() {
    // Create index
    CXIndex index = clang_createIndex(0, 0);

    // Create a simple C++ file in memory
    const char* source = R"(
int main() {
    int x = 42;
    return 0;
}
)";

    const char* filename = "test.cpp";
    CXUnsavedFile unsavedFile;
    unsavedFile.Filename = filename;
    unsavedFile.Contents = source;
    unsavedFile.Length = strlen(source);

    // Parse the translation unit
    CXTranslationUnit tu = clang_parseTranslationUnit(
        index,
        filename,
        nullptr, 0,  // command line args
        &unsavedFile, 1,  // unsaved files
        CXTranslationUnit_None
    );

    if (!tu) {
        std::cerr << "Failed to parse translation unit\n";
        clang_disposeIndex(index);
        return 1;
    }

    std::cout << "Successfully parsed translation unit\n";

    // Get the root cursor
    CXCursor cursor = clang_getTranslationUnitCursor(tu);
    CXString cursorSpelling = clang_getCursorSpelling(cursor);
    CXCursorKind cursorKind = clang_getCursorKind(cursor);
    CXString kindSpelling = clang_getCursorKindSpelling(cursorKind);

    std::cout << "Root cursor: " << clang_getCString(cursorSpelling)
              << " (kind: " << clang_getCString(kindSpelling) << ")\n";

    clang_disposeString(cursorSpelling);
    clang_disposeString(kindSpelling);

    // Cleanup
    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);

    std::cout << "libclang test completed successfully!\n";
    return 0;
}
