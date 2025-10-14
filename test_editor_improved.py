#!/usr/bin/env python3
"""
Improved test harness for VfsShell text editor
"""

import subprocess
import time
import threading
import queue
import sys
import os

class ImprovedEditorTester:
    """Better test harness that can handle interactive commands"""
    
    def __init__(self):
        self.process = None
        self.output_queue = queue.Queue()
        self.output_buffer = ""
        
    def start_shell(self):
        """Start the shell in a subprocess"""
        os.chdir("/common/active/sblo/Dev/VfsBoot/")
        
        self.process = subprocess.Popen(
            ['./codex'],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            universal_newlines=True,
            bufsize=1
        )
        
        # Start a thread to read output
        def read_output():
            while True:
                try:
                    char = self.process.stdout.read(1)
                    if char:
                        self.output_buffer += char
                        self.output_queue.put(char)
                    else:
                        break
                except:
                    break
        
        self.output_thread = threading.Thread(target=read_output, daemon=True)
        self.output_thread.start()
    
    def wait_for_output(self, timeout=1):
        """Wait for output to accumulate"""
        start_time = time.time()
        while time.time() - start_time < timeout:
            time.sleep(0.1)
            if "VfsShell Text Editor" in self.output_buffer:
                break
        return self.output_buffer
    
    def send_input(self, text):
        """Send input to the process"""
        if self.process:
            self.process.stdin.write(text + "\n")
            self.process.stdin.flush()
    
    def test_editor_appears_immediately(self):
        """Test that editor interface appears immediately"""
        print("Testing: Editor interface appears immediately")
        print("-" * 50)
        
        self.start_shell()
        time.sleep(0.2)  # Brief pause for init
        
        # Send edit command
        print("Sending: edit test.txt")
        self.send_input("edit test.txt")
        
        # Wait a bit for editor to appear
        time.sleep(0.8)
        
        # Check current output
        output = self.output_buffer
        print("Current output after 'edit test.txt':")
        print(repr(output))
        print()
        
        # Look for editor elements
        has_header = "VfsShell Text Editor" in output
        has_path = "/test.txt" in output  
        has_separator = "----" in output
        has_status = "Line:" in output and "Ctrl+X" in output
        has_prompt = "Command (" in output
        
        print("Analysis:")
        print(f"  Has 'VfsShell Text Editor': {has_header}")
        print(f"  Has file path: {has_path}")
        print(f"  Has separator: {has_separator}")
        print(f"  Has status line: {has_status}")
        print(f"  Has command prompt: {has_prompt}")
        
        success = has_header and has_path and has_separator
        if success:
            print("\n  ✅ SUCCESS: Editor interface appeared!")
            
            # Now send :q to exit
            self.send_input(":q")
            time.sleep(0.3)
            final_output = self.output_buffer
            if "Editor closed" in final_output:
                print("  ✅ Editor closed properly")
        else:
            print("\n  ❌ FAILED: Editor interface incomplete")
        
        # Terminate process
        if self.process:
            self.process.terminate()
            
        return success

    def test_full_workflow(self):
        """Test full editor workflow"""
        print("\n" + "="*60)
        print("Testing: Full editor workflow")
        print("="*60)
        
        self.output_buffer = ""  # Reset
        self.start_shell()
        time.sleep(0.2)
        
        print("1. Sending: edit workflow.txt")
        self.send_input("edit workflow.txt")
        time.sleep(0.8)
        
        # Check if editor appeared
        if "VfsShell Text Editor" in self.output_buffer:
            print("   ✅ Editor interface appeared")
        else:
            print("   ❌ Editor interface did not appear")
            self.process.terminate()
            return False
            
        # Send a command to insert text
        print("2. Sending: i1 Hello World")
        self.send_input("i1 Hello World")
        time.sleep(0.3)
        
        # Check output
        if "[Inserted line" in self.output_buffer:
            print("   ✅ Text insertion command processed")
        else:
            print("   ❌ Text insertion failed")
            
        # Read more output after insertion
        time.sleep(0.3)
        print("3. Sending: :wq to save and exit")
        self.send_input(":wq")
        time.sleep(0.5)
        
        final_output = self.output_buffer
        if "Saved" in final_output and "exited" in final_output:
            print("   ✅ File saved and editor exited successfully")
            success = True
        else:
            print("   ❌ Save/exit failed")
            print(f"   Final output: {repr(final_output[-200:])}")
            success = False
        
        if self.process:
            self.process.terminate()
            
        return success

def main():
    print("VfsShell Text Editor - Improved Test Harness")
    print("=" * 50)
    
    tester = ImprovedEditorTester()
    
    # Test 1: Does editor appear immediately?
    test1_success = tester.test_editor_appears_immediately()
    
    # Test 2: Full workflow
    test2_success = tester.test_full_workflow()
    
    print("\n" + "="*60)
    print("SUMMARY:")
    print(f"  Editor Interface Test: {'PASS' if test1_success else 'FAIL'}")
    print(f"  Full Workflow Test:    {'PASS' if test2_success else 'FAIL'}")
    print("="*60)
    
    if test1_success and test2_success:
        print("🎉 All tests PASSED! Editor is working correctly.")
    else:
        print("⚠️  Some tests FAILED. Editor needs fixes.")

if __name__ == "__main__":
    main()