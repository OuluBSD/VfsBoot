(test
  (id "test-012")
  (title "Full Integration: AI-driven Hello World in C++")
  (difficulty "medium")
  (tags "integration" "planning" "logic" "action-planner" "cpp-ast" "ai")

  (instructions
    (system "You are an AI assistant helping to create a C++ Hello World program using VfsBoot's integrated systems.")
    (task "Create a complete Hello World program in C++ using the following workflow:
1. Initialize the logic system and add C++ domain rules
2. Create a hierarchical plan with tags
3. Use logic inference to determine requirements
4. Build context for code generation
5. Generate C++ code using the AST builder
6. Validate the result

This simulates what should happen when a user types: 'ai create hello world program in c++'"))

  (prompt "Create a Hello World program in C++ using the integrated planning, logic, and code generation systems. Follow this workflow:

PHASE 1 - Logic System:
- Initialize logic with: logic.init
- Add rule: logic.rule.add cpp-iostream \"cpp\" \"needs-iostream\" 0.95 stdlib

PHASE 2 - Planning:
- Create plan: plan.create /plan/hello root \"Hello World in C++\"
- Add tags: tag.add /plan/hello cpp, tag.add /plan/hello print-text
- Infer requirements: plan.tags.infer /plan/hello
- Verify: plan.tags.check /plan/hello

PHASE 3 - Code Generation:
- Create TU: cpp.tu /astcpp/hello
- Add include: cpp.include /astcpp/hello iostream 1
- Create main: cpp.func /astcpp/hello main int
- Add output: cpp.print /astcpp/hello/main/body \"Hello World\"
- Add return: cpp.returni /astcpp/hello/main/body 0
- Dump code: cpp.dump /astcpp/hello /cpp/hello.cpp

PHASE 4 - Validation:
- Show code: cat /cpp/hello.cpp
- Validate plan: plan.validate /plan/hello

Execute all these steps.")

  (expect
    (contains "initialized logic engine")
    (contains "added rule: cpp-iostream")
    (contains "created plan node (root)")
    (contains "inferred tags")
    (contains "✓")
    (contains "cpp tu @")
    (contains "+include iostream")
    (contains "+func main")
    (contains "+print")
    (contains "+return")
    (contains "dump ->")
    (contains "#include <iostream>")
    (contains "int main()")
    (contains "std::cout")
    (contains "Hello World")
    (contains "return 0"))

  (expected-runtime-output
    (contains "Hello World"))

  (assertions
    (file-exists "/cpp/hello.cpp")
    (file-contains "/cpp/hello.cpp" "#include <iostream>")
    (file-contains "/cpp/hello.cpp" "int main()")
    (file-contains "/cpp/hello.cpp" "std::cout")
    (file-contains "/cpp/hello.cpp" "Hello World")
    (file-contains "/cpp/hello.cpp" "return 0"))

  (targets "openai" "llama"))
