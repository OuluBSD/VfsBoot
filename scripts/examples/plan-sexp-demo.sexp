; S-expression example for AI-driven planning
; This demonstrates how AI can interact with the planner system

; Create a plan for implementing a feature
(begin
  (echo "=== AI-Driven Feature Planning Demo ===")

  ; Initialize the plan
  (plan.create "/plan/feature" "root" "Add user profile feature")

  ; AI would analyze requirements and create goals
  (plan.create "/plan/feature/goals" "goals")
  (echo "goal-1: Users can upload profile pictures" >> "/plan/feature/goals")
  (echo "goal-2: Users can edit bio and personal info" >> "/plan/feature/goals")
  (echo "goal-3: Privacy controls for profile visibility" >> "/plan/feature/goals")

  ; AI determines approach
  (plan.create "/plan/feature/strategy" "strategy" "Incremental rollout: profile data model, upload system, privacy controls")

  ; AI identifies what needs research
  (plan.create "/plan/feature/research" "research")
  (echo "topic-1: Image storage and CDN options" >> "/plan/feature/research")
  (echo "topic-2: GDPR compliance for user data" >> "/plan/feature/research")
  (echo "topic-3: Profile picture size/format standards" >> "/plan/feature/research")

  ; AI breaks down into actionable jobs
  (plan.create "/plan/feature/jobs" "jobs")
  (plan.jobs.add "/plan/feature/jobs" "Design user profile schema" 5 "agent")
  (plan.jobs.add "/plan/feature/jobs" "Implement file upload endpoint" 10 "agent")
  (plan.jobs.add "/plan/feature/jobs" "Add image validation/resize" 15 "agent")
  (plan.jobs.add "/plan/feature/jobs" "Create profile edit UI" 20 "agent")
  (plan.jobs.add "/plan/feature/jobs" "Implement privacy settings" 25 "agent")
  (plan.jobs.add "/plan/feature/jobs" "Write integration tests" 30 "agent")
  (plan.jobs.add "/plan/feature/jobs" "Review security implications" 12 "user")

  ; AI identifies dependencies
  (plan.create "/plan/feature/deps" "deps")
  (echo "dep-1: File upload library (multer or similar)" >> "/plan/feature/deps")
  (echo "dep-2: Image processing library (sharp)" >> "/plan/feature/deps")
  (echo "dep-3: Cloud storage (AWS S3 or similar)" >> "/plan/feature/deps")

  ; Show the complete plan
  (echo "\n=== Complete Plan Structure ===")
  (tree "/plan/feature")

  (echo "\n=== Jobs Prioritized ===")
  (cat "/plan/feature/jobs")

  (echo "\n=== Research Topics ===")
  (cat "/plan/feature/research")

  ; Set up context for AI discussion
  (echo "\n=== Setting up AI context ===")
  (plan.goto "/plan/feature")
  (plan.context.add "/plan/feature/goals")
  (plan.context.add "/plan/feature/jobs")
  (plan.context.add "/plan/feature/research")
  (plan.forward)
  (plan.status)

  (echo "\n=== AI is ready to assist ===")
  (echo "Context includes: goals, jobs, research")
  (echo "Mode: forward (ready to elaborate on implementation)")

  ; Save for future sessions
  (plan.save)
  (echo "\nPlan saved to plan.vfs"))
