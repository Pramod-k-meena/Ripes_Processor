#!/usr/bin/env python3
import os
import subprocess
import sys
import shutil

def compare_files(file1, file2):
    try:
        with open(file1, 'r') as f1, open(file2, 'r') as f2:
            while True:
                ch1 = f1.read(1)
                ch2 = f2.read(1)
                
                if ch1 != ch2:
                    return False
                if not ch1 and not ch2:  # End of both files
                    return True
    except IOError as e:
        print(f"Failed to open files for comparison: {e}")
        return False

def test_input_file(input_file, expected_output_file):
    """
    Run the program with the given input file and compare output with expected.
    """
    try:
        # Run the no_forward program with the input file
        cmd = ["./src/noforward", input_file, "100"]
        
        with open("./outputfiles/expected_output_1.txt", 'w') as outfile, open("error.txt", 'w') as errfile:
            result = subprocess.run(
                cmd,
                stdout=outfile,
                stderr=errfile,
                text=True
            )
        
        # Check if the program ran successfully
        if result.returncode != 0:
            with open("error.txt", 'r') as errfile:
                error_content = errfile.read()
            print(f"Error running program with {input_file}: {error_content}")
            return False
        
        # Compare the output with expected
        return compare_files("./outputfiles/expected_output_1.txt", expected_output_file)
    except Exception as e:
        print(f"Error during test: {e}")
        return False

def main():
    # Define input and expected files
    input_files = [
        "./inputfiles/input1.txt",
        "./inputfiles/input2.txt",
        "./inputfiles/input3.txt",
        "./inputfiles/input4.txt",
        "./inputfiles/input5.txt",
        "./inputfiles/input6.txt",
        "./inputfiles/input7.txt",
        "./inputfiles/input8.txt",
        "./inputfiles/input9.txt"
    ]
    
    expected_files = [
        "./outputfiles/expected_output_1.txt",
        "./outputfiles/expected_output_2.txt",
        "./outputfiles/expected_output_3.txt",
        "./outputfiles/expected_output_4.txt",
        "./outputfiles/expected_output_5.txt",
        "./outputfiles/expected_output_6.txt",
        "./outputfiles/expected_output_7.txt",
        "./outputfiles/expected_output_8.txt",
        "./outputfiles/expected_output_9.txt"
    ]
    all_tests_passed = True

    for i in reversed(range(len(input_files))):
        # print(f"Testing with {input_files[i]}...")
        if test_input_file(input_files[i], expected_files[i]):
            print(f"✓ Test passed: output matches expected output {expected_files[i]}")
        else:
            print(f"✗ ERROR: output does not match expected output {expected_files[i]}")
            all_tests_passed = False
    
    if all_tests_passed:
        print("\nAll tests successful! ❤️ ❤️ ❤️ ❤️ ❤️")
    else:
        print("\nSome tests failed.🥶🥶🥶🥶🥶")
    
    return 0 if all_tests_passed else 1

if __name__ == "__main__":
    sys.exit(main())