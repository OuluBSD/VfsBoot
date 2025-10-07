(test-case
  (id "test-compile-simple")
  (title "Simple compilation test")
  (difficulty easy)
  (tags (cpp17 compile-test))
  (instructions
    (format "Write a simple hello world program using echo command")
    (tools
      (item "(cmd \"echo\" \"/path\" \"content\") to create files"))
    (workflow
      (step "Use echo to write the C++ file in one command")))
  (prompt "Create /cpp/test-simple.cpp with a main function that prints 'Compilation works!' and returns 0. Use #include <iostream> and std::cout. Use the echo command to write the file.")
  (expected-output
    (contains "(begin")
    (contains "echo")
    (contains "/cpp/test-simple.cpp"))
  (assertions
    (contains "/cpp/test-simple.cpp" "#include <iostream>")
    (contains "/cpp/test-simple.cpp" "int main()")
    (contains "/cpp/test-simple.cpp" "Compilation works!")
    (contains "/cpp/test-simple.cpp" "return 0;"))
  (expected-runtime-output
    (contains "Compilation works!"))
  (llm-targets
    (llama "http://192.168.1.169:8080/")))
