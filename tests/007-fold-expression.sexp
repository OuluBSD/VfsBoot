(test-case
  (id "007-fold-expression")
  (title "Variadic templates with fold expressions")
  (difficulty medium)
  (tags (cpp17 fold-expression variadic-template ast-workflow))
  (instructions
    (format "Provide a single (begin ... ) S-expression. Use (comment ...) for plan notes and (cmd \"<tool>\" ...) for concrete operations. Do not answer with free-form text.")
    (tools
      (item "tools, ls, mkdir, cat, vfs-write, cpp.* cover the needed surface")
      (item "When writing multi-line source, encode newlines with \n inside the string")
      (item "Cat the resulting file if you want to self-check"))
    (workflow
      (step "Inspect environment via tools and ls")
      (step "Ensure /cpp/tests exists")
      (step "Create /cpp/tests/fold_expression.cpp featuring the requested fold expression")
      (step "Return only the S-expression")))
  (prompt "Implement /cpp/tests/fold_expression.cpp containing template <typename... Ts> auto sum(Ts... values) { return (values + ...); } plus a main that calls sum with mixed numeric types and prints the results using std::cout. Finish with return 0.")
  (expected-output
    (contains "(begin")
    (contains "(cmd \"tools\")")
    (contains "(cmd \"ls\" \"/cpp\")")
    (contains "template <typename...")
    (contains "(values + ...)")
    (contains "sum(")
    (contains "std::cout")
    (contains ")"))
  (assertions
    (contains "/cpp/tests/fold_expression.cpp" "template <typename... Ts>")
    (contains "/cpp/tests/fold_expression.cpp" "(values + ...)")
    (contains "/cpp/tests/fold_expression.cpp" "sum(1, 2, 3)")
    (contains "/cpp/tests/fold_expression.cpp" "std::cout << \"sum\"")
    (contains "/cpp/tests/fold_expression.cpp" "return 0;")
    (not-contains "/cpp/tests/fold_expression.cpp" "TODO"))
  (expected-runtime-output
    (contains "sum")
    (contains "6"))
  (llm-targets
    (openai default)
    (llama "http://192.168.1.169:8080/")))
