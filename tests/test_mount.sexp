; Test filesystem and library mounting
; This test demonstrates mounting host filesystem and shared libraries

(test-name "mount-filesystem-and-library")
(test-description "Mount host directories and .so libraries into VFS")

(commands
  ; Create mount points
  "mkdir /mnt"
  "mkdir /dev"

  ; Test mount.list before mounting
  "mount.list"

  ; Mount the tests directory
  "mount tests /mnt/tests"

  ; List the mounted directory
  "ls /mnt/tests"

  ; Mount the test library
  "mount.lib tests/libtest.so /dev/testlib"

  ; List library contents (should show _info)
  "ls /dev/testlib"

  ; Read library info
  "cat /dev/testlib/_info"

  ; List all mounts
  "mount.list"

  ; Test mount.disallow
  "mount.disallow"

  ; Try to mount while disallowed (should fail)
  ; "mount /tmp /mnt/tmp"  ; Uncomment to test error handling

  ; Re-enable mounting
  "mount.allow"

  ; Unmount
  "unmount /mnt/tests"
  "unmount /dev/testlib"

  ; Verify mounts are gone
  "mount.list"

  "exit"
)

(expected-output "")
(expected-runtime-output
  (contains "mounted tests -> /mnt/tests")
  (contains "mounted library")
  (contains "Library loaded")
  (contains "mounting allowed")
  (contains "mounting disabled")
  (contains "unmounted /mnt/tests")
)
