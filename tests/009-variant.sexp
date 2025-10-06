(test-case
  (id "009-variant")
  (title "std::variant with visitors")
  (difficulty hard)
  (tags (cpp17 variant visit pattern-matching ast-workflow))
  (instructions
    (format "Return exactly one (begin ... ) S-expression. Use (comment ...) for optional breadcrumbs and (cmd \"<tool>\" ...) for each action. Avoid any free-floating text.")
    (tools
      (item "Use tools/ls/mkdir/cat to explore; write code with vfs-write or cpp.*")
      (item "Remember to escape double quotes inside strings")
      (item "Optionally inspect the result with cat"))
    (workflow
      (step "Run (cmd \"tools\") then (cmd \"ls\" \"/cpp\") to orient")
      (step "Ensure /cpp/tests exists")
      (step "Write /cpp/tests/variant.cpp per the instructions")
      (step "Respond only with the S-expression")))
  (prompt "Author /cpp/tests/variant.cpp that defines std::variant<int, std::string, double> values, visits them with std::visit using an overloaded visitor struct (operator() overloads) to print descriptive messages via std::cout. Include a main that iterates a container of variant values, invokes the visitor, and returns 0.")
  (expected-output
    (contains "(begin")
    (contains "(cmd \"tools\")")
    (contains "(cmd \"ls\" \"/cpp\")")
    (contains "std::variant<int, std::string, double>")
    (contains "std::visit")
    (contains "struct Overloaded")
    (contains "operator()")
    (contains "std::cout")
    (contains ")"))
  (assertions
    (contains "/cpp/tests/variant.cpp" "#include <variant>")
    (contains "/cpp/tests/variant.cpp" "struct Overloaded")
    (contains "/cpp/tests/variant.cpp" "std::visit(")
    (contains "/cpp/tests/variant.cpp" "std::cout << \"int\"")
    (contains "/cpp/tests/variant.cpp" "return 0;")
    (not-contains "/cpp/tests/variant.cpp" "TODO"))
  (llm-targets
    (openai default)
    (llama "http://192.168.1.169:8080/")))
