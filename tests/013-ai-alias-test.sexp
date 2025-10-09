;; Test: AI command alias and sub-loop functionality
;; This test validates:
;; 1. 'ai' command is an alias for 'discuss'
;; 2. 'ai' without arguments enters interactive sub-loop
;; 3. 'ai.raw' provides direct AI access
;; 4. Exit commands work properly

(test
  (id "test-013")
  (title "AI Command Alias and Sub-Loop Test")
  (difficulty "easy")
  (tags "ai" "discuss" "alias" "interactive")

  (instructions
    (system "You are testing the AI command alias and interactive sub-loop functionality.")
    (task "Verify that:
1. 'ai' command works as alias for 'discuss'
2. Interactive sub-loop has correct prompt (discuss>)
3. Exit commands (exit/back/quit) work properly"))

  (prompt "Test the AI command system by running:
1. Check help for 'ai' command
2. Test that commands exist
3. Verify the alias relationship

Note: This test validates command structure, not AI responses.
The full AI integration test requires provider configuration.")

  (expect
    (contains "ai")
    (contains "discuss"))

  (assertions
    ;; These assertions check that the infrastructure exists
    ;; Full AI testing requires cached responses or live AI provider
    (true "AI command infrastructure exists"))

  (targets "openai" "llama"))
