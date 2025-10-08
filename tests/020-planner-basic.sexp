; Test basic planner functionality
; Tests plan creation, jobs, navigation, and persistence

; Create a test plan
plan.create /plan/test root "Test plan"
plan.create /plan/test/goals goals
plan.create /plan/test/jobs jobs
plan.create /plan/test/strategy strategy "Test strategy"

; Add some goals
echo goal-1: First goal >> /plan/test/goals
echo goal-2: Second goal >> /plan/test/goals

; Add jobs with priorities
plan.jobs.add /plan/test/jobs "High priority task" 10 agent
plan.jobs.add /plan/test/jobs "Low priority task" 50 user
plan.jobs.add /plan/test/jobs "Medium priority task" 25 agent

; Test navigation
plan.goto /plan/test
plan.status

; Test forward/backward
plan.forward
plan.backward

; Test context management
plan.context.add /plan/test/goals
plan.context.add /plan/test/jobs
plan.context.list
plan.context.remove /plan/test/goals
plan.context.clear

; Complete a job
plan.jobs.complete /plan/test/jobs 0

; Check structure
tree /plan/test

; Check jobs display (should show priority sorting)
cat /plan/test/jobs

; Verify goals
cat /plan/test/goals
