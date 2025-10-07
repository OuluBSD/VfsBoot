(test-case
  (id "006-if-constexpr")
  (title "if constexpr dispatch in templates")
  (difficulty medium)
  (tags (cpp17 if-constexpr type-traits templates ast-workflow))
  (instructions
    (format "Respond using a single (begin ... ) S-expression. Commentary goes in (comment ...); real actions are (cmd \"<tool>\" ...). Nothing outside the form.")
    (tools
      (item "Reach for tools, ls, mkdir, cat, vfs-write, cpp.* as required")
      (item "Embed multi-line C++ in a vfs-write string with \n escapes")
      (item "You may call cat to review the result"))
    (workflow
      (step "Probe with (cmd \"tools\") then (cmd \"ls\" \"/cpp\")")
      (step "Make sure /cpp/tests exists")
      (step "Write /cpp/tests/if_constexpr.cpp containing the described template example")
      (step "Return only the S-expression")))
  (prompt "Craft /cpp/tests/if_constexpr.cpp that defines template <typename T> void describe(const T&) leveraging if constexpr with std::is_integral_v and std::is_floating_point_v to branch at compile time. main() should call describe with examples (int, double, std::string) and return 0. Print messages via std::cout.")
  (expected-output
    (contains "(begin")
    (contains "(cmd \"tools\")")
    (contains "(cmd \"ls\" \"/cpp\")")
    (contains "if constexpr")
    (contains "std::is_integral")
    (contains "describe(")
    (contains "std::cout")
    (contains ")"))
  (assertions
    (contains "/cpp/tests/if_constexpr.cpp" "#include <type_traits>")
    (contains "/cpp/tests/if_constexpr.cpp" "template <typename T>")
    (contains "/cpp/tests/if_constexpr.cpp" "if constexpr")
    (contains "/cpp/tests/if_constexpr.cpp" "std::is_integral_v<T>")
    (contains "/cpp/tests/if_constexpr.cpp" "std::cout << \"integral\"")
    (contains "/cpp/tests/if_constexpr.cpp" "return 0;")
    (not-contains "/cpp/tests/if_constexpr.cpp" "TODO"))
  (expected-runtime-output
    (contains "integral"))
  (llm-targets
    (openai default)
    (llama "http://192.168.1.169:8080/")))
