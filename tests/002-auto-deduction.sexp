(test-case
  (id "002-auto-deduction")
  (title "auto deduction with uniform initialization")
  (difficulty easy)
  (tags (cpp17 auto range-for ast-workflow))
  (instructions
    (format "Respond with one (begin ... ) S-expression. Use (comment ...) for narration and (cmd \"<tool>\" ...) for actions. Keep all activity inside the form; no prose responses.")
    (tools
      (item "Essential commands: tools, ls, mkdir, cat, vfs-write, cpp.dump if you build via AST")
      (item "vfs-write expects the entire file payload as a single string (use \n for newlines)")
      (item "Feel free to reuse cpp.* helpers from Test 001 if convenient"))
    (workflow
      (step "List directories with (cmd \"tools\") then (cmd \"ls\" \"/cpp\")")
      (step "Ensure /cpp/tests exists via (cmd \"mkdir\" \"/cpp/tests\")")
      (step "Write or update the translation unit with (cmd \"vfs-write\" \"/cpp/tests/auto.cpp\" \"...\")")
      (step "Optionally (cmd \"cat\" \"/cpp/tests/auto.cpp\") to confirm contents")))
  (prompt "Create /cpp/tests/auto.cpp demonstrating C++17 auto deduction: use uniform initialization for a std::vector<int>, compute a sum with a range-based for loop, and print the total with std::cout. Return 0 from main. Include any necessary headers.")
  (expected-output
    (contains "(begin")
    (contains "(cmd \"tools\")")
    (contains "(cmd \"ls\" \"/cpp\")")
    (contains "auto total")
    (contains "for (auto value :")
    (contains "std::cout")
    (contains ")"))
  (assertions
    (contains "/cpp/tests/auto.cpp" "#include <vector>")
    (contains "/cpp/tests/auto.cpp" "std::vector<int> values{1, 2, 3, 4, 5};")
    (contains "/cpp/tests/auto.cpp" "auto total")
    (contains "/cpp/tests/auto.cpp" "for (auto value : values)")
    (contains "/cpp/tests/auto.cpp" "std::cout << \"total\"")
    (contains "/cpp/tests/auto.cpp" "return 0;")
    (not-contains "/cpp/tests/auto.cpp" "TODO"))
  (expected-runtime-output
    (contains "total")
    (contains "15"))
  (llm-targets
    (openai default)
    (llama "http://192.168.1.169:8080/")))
