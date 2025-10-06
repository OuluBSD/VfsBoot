(test-case
  (id "010-filesystem")
  (title "std::filesystem traversal and metadata")
  (difficulty hard)
  (tags (cpp17 filesystem directory-iteration error-handling ast-workflow))
  (instructions
    (format "Reply with a single (begin ... ) S-expression. Keep commentary inside (comment ...) nodes and drive the workflow via (cmd \"<tool>\" ...). No stray prose.")
    (tools
      (item "Use tools/ls/mkdir/cat to inspect the VFS; write outputs with vfs-write or cpp.*")
      (item "Escape embedded quotes when writing strings")
      (item "filesystem paths may require doubling backslashes only if you introduce Windows-style paths; stick to POSIX where possible"))
    (workflow
      (step "Call (cmd \"tools\") then (cmd \"ls\" \"/cpp\") to orient")
      (step "Ensure /cpp/tests is available")
      (step "Write /cpp/tests/filesystem.cpp implementing the required traversal")
      (step "Reply with the S-expression only")))
  (prompt "Compose /cpp/tests/filesystem.cpp that uses std::filesystem to create a temporary subdirectory under std::filesystem::temp_directory_path(), writes a couple of files into it, iterates entries with directory_iterator, and prints each path plus file size. Handle errors (e.g., wrap operations in try/catch) and return 0 from main.")
  (expected-output
    (contains "(begin")
    (contains "(cmd \"tools\")")
    (contains "(cmd \"ls\" \"/cpp\")")
    (contains "std::filesystem::temp_directory_path")
    (contains "std::filesystem::create_directories")
    (contains "std::filesystem::directory_iterator")
    (contains "std::cout")
    (contains ")"))
  (assertions
    (contains "/cpp/tests/filesystem.cpp" "#include <filesystem>")
    (contains "/cpp/tests/filesystem.cpp" "std::filesystem::temp_directory_path")
    (contains "/cpp/tests/filesystem.cpp" "std::filesystem::create_directories")
    (contains "/cpp/tests/filesystem.cpp" "for (const auto& entry : std::filesystem::directory_iterator")
    (contains "/cpp/tests/filesystem.cpp" "std::cout << entry.path()")
    (contains "/cpp/tests/filesystem.cpp" "return 0;")
    (not-contains "/cpp/tests/filesystem.cpp" "TODO"))
  (llm-targets
    (openai default)
    (llama "http://192.168.1.169:8080/")))
