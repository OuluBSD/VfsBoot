(test-case
  (id "005-structured-bindings")
  (title "Structured bindings over associative containers")
  (difficulty medium)
  (tags (cpp17 structured-bindings map range-for ast-workflow))
  (instructions
    (format "Answer with a single (begin ... ) S-expression. Combine optional (comment ...) breadcrumbs with (cmd \"<tool>\" ...) actions. Nothing outside the form.")
    (tools
      (item "Use tools/ls/mkdir/cat to navigate; write files via vfs-write or cpp.dump")
      (item "Remember to escape quotes inside strings (\"like this\")")
      (item "Check the file with cat if helpful"))
    (workflow
      (step "Inspect environment with tools and ls")
      (step "Guarantee /cpp/tests exists")
      (step "Author /cpp/tests/structured_bindings.cpp implementing the scenario")
      (step "Return only the S-expression")))
  (prompt "Produce /cpp/tests/structured_bindings.cpp showcasing C++17 structured bindings. Populate a std::map<std::string, int>, iterate it with for (const auto& [key, value] : map), print each pair, and compute summary statistics (e.g., total count / sum). Use std::cout for output and return 0 from main.")
  (expected-output
    (contains "(begin")
    (contains "(cmd \"tools\")")
    (contains "(cmd \"ls\" \"/cpp\")")
    (contains "std::map")
    (contains "[key, value]")
    (contains "structured")
    (contains "std::cout")
    (contains ")"))
  (assertions
    (contains "/cpp/tests/structured_bindings.cpp" "#include <map>")
    (contains "/cpp/tests/structured_bindings.cpp" "for (const auto& [key, value] :")
    (contains "/cpp/tests/structured_bindings.cpp" "std::cout << key")
    (contains "/cpp/tests/structured_bindings.cpp" "return 0;")
    (not-contains "/cpp/tests/structured_bindings.cpp" "TODO"))
  (llm-targets
    (openai default)
    (llama "http://192.168.1.169:8080/")))
