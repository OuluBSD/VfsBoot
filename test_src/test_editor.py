#!/usr/bin/env python3
"""
Terminal emulator test harness for VfsShell text editor
This script provides a client-server architecture to interact with the editor
and examine its screen buffer and behavior.
"""

import socket
import threading
import time
import select
import sys
import subprocess
import os
from io import StringIO

class TerminalEmulator:
    """A simple terminal emulator to interact with the text editor."""
    
    def __init__(self):
        self.screen_buffer = []
        self.current_line = ""
        self.cursor_pos = 0
        self.width = 80
        self.height = 24
        self.process = None
        self.input_buffer = []
        self.output_buffer = []
        
    def start_vfs_shell(self):
        """Start the vfs shell process"""
        # Change to the project directory
        os.chdir("/common/active/sblo/Dev/VfsBoot/")
        
        # Start the codex process
        self.process = subprocess.Popen(
            ['./codex'],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=0  # Unbuffered
        )
        
    def send_command(self, command):
        """Send a command to the shell and return the response"""
        if self.process:
            # Send command with newline
            self.process.stdin.write(command + "\n")
            self.process.stdin.flush()
            
            # Give some time for processing
            time.sleep(0.1)
            
            # Try to read output (non-blocking)
            output = ""
            try:
                # Attempt to read available output
                while True:
                    ready, _, _ = select.select([self.process.stdout], [], [], 0.1)
                    if ready:
                        char = self.process.stdout.read(1)
                        if char:
                            output += char
                        else:
                            break
                    else:
                        break
            except:
                pass
                
            return output
        return ""

class EditorTester:
    """Test harness for the VfsShell text editor"""
    
    def __init__(self):
        self.terminal = TerminalEmulator()
        
    def test_basic_editor(self):
        """Test basic editor functionality"""
        print("Testing basic editor functionality...")
        
        # Start the shell
        self.terminal.start_vfs_shell()
        time.sleep(0.5)  # Give it time to start
        
        print("Shell started, sending 'edit test.txt' command...")
        
        # Try to run the edit command
        output = self.terminal.send_command("edit test.txt")
        print("Output after 'edit test.txt':")
        print(repr(output))
        
        # Wait a bit more to see the editor interface
        time.sleep(1)
        
        print("\nTrying to send editor commands...")
        output2 = self.terminal.send_command("help")
        print("Output after 'help':")
        print(repr(output2))
        
        # Exit 
        output3 = self.terminal.send_command(":q")
        print("Output after ':q':")
        print(repr(output3))
        
        # Close the process
        if self.terminal.process:
            self.terminal.process.terminate()
    
    def test_editor_interface_immediately(self):
        """Test that editor interface appears immediately"""
        print("\n" + "="*50)
        print("Testing: Editor interface should appear immediately")
        print("="*50)
        
        # Start a new shell
        self.terminal = TerminalEmulator()
        self.terminal.start_vfs_shell()
        time.sleep(0.3)
        
        # Send the edit command and immediately check for editor interface
        self.terminal.process.stdin.write("edit hello.txt\n")
        self.terminal.process.stdin.flush()
        
        # Wait briefly for the editor to initialize
        time.sleep(0.5)
        
        # Read what's available - we should see the editor interface
        output = ""
        try:
            # Read all available output
            while True:
                ready, _, _ = select.select([self.terminal.process.stdout], [], [], 0.1)
                if ready:
                    char = self.terminal.process.stdout.read(1)
                    if char:
                        output += char
                    else:
                        break
                else:
                    break
        except:
            pass
        
        print("Immediate output after 'edit hello.txt':")
        print(repr(output))
        
        # Check if editor interface is present
        has_header = "VfsShell Text Editor" in output
        has_separator = "----" in output
        has_status = "Line:" in output
        
        print(f"  Found 'VfsShell Text Editor': {has_header}")
        print(f"  Found separator: {has_separator}")
        print(f"  Found status line: {has_status}")
        
        if has_header:
            print("  ✅ SUCCESS: Editor interface appeared immediately!")
        else:
            print("  ❌ FAILED: Editor interface did not appear immediately")
        
        # Clean up
        try:
            self.terminal.process.stdin.write(":q\n")
            self.terminal.process.stdin.flush()
            time.sleep(0.1)
        except:
            pass
        
        if self.terminal.process:
            self.terminal.process.terminate()

def main():
    print("VfsShell Text Editor Test Harness")
    print("=" * 40)
    
    tester = EditorTester()
    
    # Run the test
    tester.test_editor_interface_immediately()
    
    print("\nTest completed!")

if __name__ == "__main__":
    main()