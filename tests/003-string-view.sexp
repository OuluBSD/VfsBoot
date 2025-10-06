(test-case
  (id "003-string-view")
  (title "string_view helpers with constexpr analysis")
  (difficulty medium)
  (tags (cpp17 string-view constexpr parsing ast-workflow))
  (instructions
    (format "Emit a single (begin ... ) S-expression. Use (comment ...) sparingly for narration and (cmd \"<tool>\" ...) for concrete commands. All actions—including inspections—happen inside that form.")
    (tools
      (item "Common commands: tools, ls, cat, mkdir, vfs-write, cpp.* as needed")
      (item "Embed newlines in vfs-write payloads with \n. Prefer std::string_view for literal views.")
      (item "You may cat the file after writing to verify"))
    (workflow
      (step "Call (cmd \"tools\") and (cmd \"ls\" \"/cpp\") to orient yourself")
      (step "Ensure /cpp/tests exists (mkdir is idempotent)")
      (step "Write /cpp/tests/string_view.cpp containing the requested program")
      (step "Final command can be (cmd \"cat\" ...) or leave as-is")))
  (prompt "Author /cpp/tests/string_view.cpp using C++17 std::string_view. Provide a constexpr count_words(std::string_view) that counts whitespace-separated tokens. In main, evaluate the helper on a few sample phrases, printing both the phrase and its token count with std::cout. End main with return 0.")
  (expected-output
    (contains "(begin")
    (contains "(cmd \"tools\")")
    (contains "(cmd \"ls\" \"/cpp\")")
    (contains "std::string_view")
    (contains "constexpr")
    (contains "count_words")
    (contains "std::cout")
    (contains ")"))
  (assertions
    (contains "/cpp/tests/string_view.cpp" "#include <string_view>")
    (contains "/cpp/tests/string_view.cpp" "constexpr std::size_t count_words(std::string_view")
    (contains "/cpp/tests/string_view.cpp" "std::cout << phrase")
    (contains "/cpp/tests/string_view.cpp" "return 0;")
    (not-contains "/cpp/tests/string_view.cpp" "TODO"))
  (llm-targets
    (openai default)
    (llama "http://192.168.1.169:8080/")))
