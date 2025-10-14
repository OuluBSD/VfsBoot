#!/usr/bin/env python3
"""
Improved terminal emulator test harness for VfsShell text editor
Uses pty to provide real terminal interface for proper ANSI escape sequence support
"""

import pty
import os
import signal
import time
import select
import termios
import tty
import threading
from io import StringIO

class PseudoTerminalEditorTester:
    """Test harness using PTY for proper terminal support"""
    
    def __init__(self):
        self.master_fd = None
        self.slave_fd = None
        self.process_pid = None
        self.output_buffer = ""
        self.all_output = ""
    
    def setup_pty(self):
        """Setup a pseudo-terminal for the process"""
        self.master_fd, self.slave_fd = pty.openpty()
        
        # Start the process connected to the slave side of pty
        pid = os.fork()
        if pid == 0:  # Child process
            os.close(self.master_fd)
            os.dup2(self.slave_fd, 0)  # stdin
            os.dup2(self.slave_fd, 1)  # stdout
            os.dup2(self.slave_fd, 2)  # stderr
            os.close(self.slave_fd)
            
            # Change to project directory and exec
            os.chdir("/common/active/sblo/Dev/VfsBoot/")
            os.execv("./codex", ["codex"])
        else:  # Parent process
            os.close(self.slave_fd)
            self.process_pid = pid
    
    def send_command(self, command):
        """Send a command to the process through the pty"""
        if self.master_fd:
            os.write(self.master_fd, (command + "\n").encode())
    
    def read_output(self, timeout=0.5):
        """Read output from the process"""
        output = ""
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            ready, _, _ = select.select([self.master_fd], [], [], 0.1)
            if ready:
                try:
                    data = os.read(self.master_fd, 1024)
                    if data:
                        chunk = data.decode('utf-8', errors='replace')
                        output += chunk
                        self.all_output += chunk
                    else:
                        break
                except OSError:
                    break
            else:
                break
        
        return output
    
    def close(self):
        """Close the pty and process"""
        if self.process_pid:
            try:
                os.kill(self.process_pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
        if self.master_fd:
            os.close(self.master_fd)

    def test_editor_interface_immediately(self):
        """Test that editor interface appears immediately"""
        print("Testing: Editor interface appears immediately (with PTY)")
        print("-" * 60)
        
        try:
            self.setup_pty()
            time.sleep(0.3)  # Wait for shell to start
            
            print("Sending: edit pty_test.txt")
            self.send_command("edit pty_test.txt")
            
            # Wait to capture editor interface
            time.sleep(0.8)
            
            output = self.read_output(1.0)
            combined_output = self.all_output
            
            print("Combined output after 'edit pty_test.txt':")
            print(repr(combined_output[-500:]))  # Show last 500 chars
            print()
            
            # Look for editor elements
            has_header = "VfsShell Text Editor" in combined_output
            has_path = "pty_test.txt" in combined_output  
            has_separator = "----" in combined_output
            has_status = "Line:" in combined_output and "Ctrl+X" in combined_output
            has_command_prompt = "Command (" in combined_output
            
            print("Analysis:")
            print(f"  Has 'VfsShell Text Editor': {has_header}")
            print(f"  Has file path: {has_path}")
            print(f"  Has separator: {has_separator}")
            print(f"  Has status line: {has_status}")
            print(f"  Has command prompt: {has_command_prompt}")
            
            success = has_header and has_path
            if success:
                print("\n  ✅ SUCCESS: Editor interface appeared!")
                
                # Now try to send an editor command to make sure it continues working
                self.send_command(":q")  # Quit command
                time.sleep(0.3)
                final_output = self.all_output
                if "Editor closed" in final_output or "Return to shell" in final_output:
                    print("  ✅ Editor quit successfully")
                else:
                    print("  ? Editor may not have quit properly")
            else:
                print("\n  ❌ FAILED: Editor interface incomplete")
                
            return success
            
        finally:
            self.close()

def main():
    print("VfsShell Text Editor - PTY Test Harness")
    print("=" * 50)
    
    tester = PseudoTerminalEditorTester()
    
    success = tester.test_editor_interface_immediately()
    
    print("\n" + "="*50)
    print(f"RESULT: {'PASS' if success else 'FAIL'}")
    print("="*50)

if __name__ == "__main__":
    main()