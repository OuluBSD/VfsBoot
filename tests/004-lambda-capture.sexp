(test-case
  (id "004-lambda-capture")
  (title "Lambda captures and higher-order utilities")
  (difficulty medium)
  (tags (cpp17 lambda-capture std-function algorithms ast-workflow))
  (instructions
    (format "Return a single (begin ... ) S-expression. Narrate with (comment ...) if desired; execute actions with (cmd \"<tool>\" ...). No plain-text chat outside the form.")
    (tools
      (item "Core tools: tools, ls, mkdir, cat, vfs-write, cpp.*")
      (item "Place generated source in /cpp/tests. Use \n inside string literals when writing multi-line files.")
      (item "(cmd \"cat\" ...) is handy to double-check the result but optional"))
    (workflow
      (step "Discover environment via (cmd \"tools\") and (cmd \"ls\" \"/cpp\")")
      (step "Ensure the /cpp/tests directory exists")
      (step "Write /cpp/tests/lambda_capture.cpp per the task using vfs-write or cpp.dump")
      (step "Return the S-expression only")))
  (prompt "Create /cpp/tests/lambda_capture.cpp demonstrating C++17 lambda captures. The program should store a stateful lambda in std::function, apply it with std::transform to a vector of ints (e.g., scaling and shifting values), and then accumulate the transformed values with std::accumulate or a loop, printing both the transformed list and its sum via std::cout. Finish main with return 0.")
  (expected-output
    (contains "(begin")
    (contains "(cmd \"tools\")")
    (contains "(cmd \"ls\" \"/cpp\")")
    (contains "std::function")
    (contains "std::transform")
    (contains "lambda")
    (contains "std::cout")
    (contains ")"))
  (assertions
    (contains "/cpp/tests/lambda_capture.cpp" "#include <functional>")
    (contains "/cpp/tests/lambda_capture.cpp" "#include <algorithm>")
    (contains "/cpp/tests/lambda_capture.cpp" "std::function<int(int)>")
    (contains "/cpp/tests/lambda_capture.cpp" "std::transform")
    (contains "/cpp/tests/lambda_capture.cpp" "std::cout << \"transformed\"")
    (contains "/cpp/tests/lambda_capture.cpp" "return 0;")
    (not-contains "/cpp/tests/lambda_capture.cpp" "TODO"))
  (expected-runtime-output
    (contains "transformed"))
  (llm-targets
    (openai default)
    (llama "http://192.168.1.169:8080/")))
