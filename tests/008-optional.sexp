(test-case
  (id "008-optional")
  (title "std::optional pipeline")
  (difficulty medium)
  (tags (cpp17 optional error-handling ast-workflow))
  (instructions
    (format "Deliver a single (begin ... ) S-expression. Use (comment ...) only for brief narration; actual work happens through (cmd \"<tool>\" ...). No extra prose.")
    (tools
      (item "Navigate with tools, ls, mkdir, cat. Write full files via vfs-write or cpp.dump")
      (item "Escape embedded quotes as \"value\" inside strings")
      (item "Cat the result if you want a sanity check"))
    (workflow
      (step "Start with (cmd \"tools\") and (cmd \"ls\" \"/cpp\")")
      (step "Ensure /cpp/tests exists")
      (step "Author /cpp/tests/optional.cpp per the brief")
      (step "Only return the S-expression")))
  (prompt "Construct /cpp/tests/optional.cpp containing a parse_int(std::string_view) -> std::optional<int> helper that returns std::nullopt on failure. main() should exercise both success and failure paths, using has_value() / value() and printing messages through std::cout. End with return 0.")
  (expected-output
    (contains "(begin")
    (contains "(cmd \"tools\")")
    (contains "(cmd \"ls\" \"/cpp\")")
    (contains "std::optional")
    (contains "has_value")
    (contains "value()")
    (contains "std::cout")
    (contains ")"))
  (assertions
    (contains "/cpp/tests/optional.cpp" "#include <optional>")
    (contains "/cpp/tests/optional.cpp" "std::optional<int> parse_int")
    (contains "/cpp/tests/optional.cpp" "if (!result.has_value())")
    (contains "/cpp/tests/optional.cpp" "result.value()")
    (contains "/cpp/tests/optional.cpp" "std::cout << \"parsed\"")
    (contains "/cpp/tests/optional.cpp" "return 0;")
    (not-contains "/cpp/tests/optional.cpp" "TODO"))
  (expected-runtime-output
    (contains "parsed"))
  (llm-targets
    (openai default)
    (llama "http://192.168.1.169:8080/")))
