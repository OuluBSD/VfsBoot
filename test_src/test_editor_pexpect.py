#!/usr/bin/env python3
"""
Proper test harness using pexpect for terminal interaction testing
"""

import pexpect
import tempfile
import os
import sys

def test_editor_in_real_terminal():
    """Test the editor using pexpect which handles real terminal interaction"""
    print("Testing VfsShell Text Editor with pexpect...")
    print("=" * 60)
    
    try:
        # Change to the project directory
        os.chdir("/common/active/sblo/Dev/VfsBoot/")
        
        # Spawn the codex process
        child = pexpect.spawn('./codex')
        child.logfile = sys.stdout.buffer  # Log for debugging
        
        # Wait for the shell prompt
        child.expect('> ', timeout=5)
        print("Shell prompt received")
        
        # Send the edit command
        print("Sending: edit expect_test.txt")
        child.sendline('edit expect_test.txt')
        
        # Wait for the editor interface to appear
        child.expect('VfsShell Text Editor', timeout=5)
        print("✅ Editor interface appeared immediately!")
        
        # Wait for the file path to be shown
        child.expect('/expect_test.txt', timeout=2)
        print("✅ File path shown in header!")
        
        # Wait for the separator line
        child.expect('============================================================', timeout=2)
        print("✅ Editor separator displayed!")
        
        # Wait for the editor prompt
        child.expect('Editor> ', timeout=2)
        print("✅ Editor prompt appeared!")
        
        # Test an insert command
        print("Testing: i1 Hello from pexpect test")
        child.sendline('i1 Hello from pexpect test')
        
        # Check for success message
        child.expect('\\[Inserted line 1\\]', timeout=2)
        print("✅ Insert command worked!")
        
        # Test print command to refresh
        print("Testing: p command to show content")
        child.sendline('p')
        
        # Wait for editor to refresh with new content
        child.expect('VfsShell Text Editor', timeout=3)
        child.expect('Hello from pexpect test', timeout=2)
        print("✅ Content properly displayed!")
        
        # Save and quit
        print("Exiting: :wq")
        child.sendline(':wq')
        
        # Wait for success message
        child.expect('Saved', timeout=3)
        child.expect('Editor closed', timeout=2)
        print("✅ File saved and editor exited successfully!")
        
        # Close the process
        child.expect(pexpect.EOF, timeout=2)
        
        print("\n" + "=" * 60)
        print("🎉 ALL TESTS PASSED! Editor works in real terminal environment.")
        print("=" * 60)
        return True
        
    except pexpect.exceptions.TIMEOUT:
        print("\n❌ TIMEOUT: Editor interface didn't appear as expected")
        return False
    except pexpect.exceptions.EOF:
        print("\n❌ EOF: Process ended unexpectedly")
        return False
    except Exception as e:
        print(f"\n❌ ERROR: {e}")
        # Try to print the last few lines of output if child exists
        try:
            print("Last output:", child.before.decode() if hasattr(child, 'before') and child.before else "No output")
        except:
            pass
        return False
    finally:
        try:
            child.close()
        except:
            pass

def test_editor_without_filename():
    """Test the editor without a filename parameter"""
    print("\nTesting Editor without filename parameter...")
    print("-" * 50)
    
    try:
        os.chdir("/common/active/sblo/Dev/VfsBoot/")
        child = pexpect.spawn('./codex')
        
        # Wait for shell prompt
        child.expect('> ', timeout=5)
        
        # Send edit command without filename
        print("Sending: ee (no filename)")
        child.sendline('ee')
        
        # Should get filename prompt
        child.expect('Enter filename to save', timeout=3)
        print("✅ Prompt for filename appeared!")
        
        # Provide a filename
        child.sendline('no_param_test.txt')
        print("Provided filename: no_param_test.txt")
        
        # Wait for editor interface
        child.expect('VfsShell Text Editor', timeout=5)
        child.expect('no_param_test.txt', timeout=2)
        print("✅ Editor appeared with provided filename!")
        
        # Test typing something
        child.sendline('i1 Test without parameter')
        child.expect('\\[Inserted line 1\\]', timeout=2)
        print("✅ Content insertion worked!")
        
        # Save and exit
        child.sendline(':wq')
        child.expect('Saved', timeout=3)
        print("✅ File saved successfully!")
        
        child.expect(pexpect.EOF, timeout=2)
        
        print("✅ Editor without filename parameter works!")
        return True
        
    except Exception as e:
        print(f"❌ Test failed: {e}")
        try:
            print("Last output:", child.before.decode() if hasattr(child, 'before') and child.before else "No output")
        except:
            pass
        return False
    finally:
        try:
            child.close()
        except:
            pass

def main():
    print("VfsShell Text Editor - Pexpect Test Harness")
    print("=" * 60)
    
    # Check if pexpect is available
    try:
        import pexpect
    except ImportError:
        print("❌ pexpect library not found! Install with: pip install pexpect")
        return 1
    
    # Run tests
    test1_success = test_editor_in_real_terminal()
    test2_success = test_editor_without_filename()  # Only if first test passed
    
    print("\n" + "=" * 60)
    print("FINAL RESULTS:")
    print(f"  Basic Editor Test:     {'✅ PASS' if test1_success else '❌ FAIL'}")
    print(f"  No Parameter Test:     {'✅ PASS' if test2_success else '❌ FAIL'}")
    print("=" * 60)
    
    if test1_success and test2_success:
        print("🎉 ALL TESTS PASSED!")
        return 0
    else:
        print("⚠️  Some tests failed.")
        return 1

if __name__ == "__main__":
    exit(main())