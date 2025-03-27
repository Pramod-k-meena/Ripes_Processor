#!/usr/bin/env python3
import os
import subprocess
import sys
import glob
import time

def process_input_files(input_dir, src_dir, output_dir):
    # Validate input directories exist
    if not os.path.exists(input_dir):
        print(f"Error: Input directory {input_dir} does not exist.")
        sys.exit(1)
    
    if not os.path.exists(src_dir):
        print(f"Error: Source directory {src_dir} does not exist.")
        sys.exit(1)
    
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Path to executables
    forward_exe = os.path.join(src_dir, 'forward')
    noforward_exe = os.path.join(src_dir, 'noforward')
    
    # Validate executables exist
    if not os.path.exists(forward_exe):
        print(f"Error: forward executable not found in {src_dir}")
        sys.exit(1)
    
    if not os.path.exists(noforward_exe):
        print(f"Error: noforward executable not found in {src_dir}")
        sys.exit(1)
    
    # Process each .txt file
    for filename in os.listdir(input_dir):
        if filename.endswith('.txt'):
            input_file_path = os.path.join(input_dir, filename)
            base_filename = os.path.splitext(filename)[0]
            
            # Clear previous output files that might conflict
            for old_file in glob.glob(f"{output_dir}/{base_filename}_*_out.txt"):
                os.remove(old_file)
            
            # Run forward executable with full path to output directory
            try:
                print(f"Processing {filename} with forward...")
                env = os.environ.copy()
                env["OUTPUT_DIR"] = output_dir  # Pass output dir as environment variable
                
                forward_output = subprocess.run(
                    [forward_exe, input_file_path, '50'], 
                    capture_output=True, 
                    text=True, 
                    env=env,
                    cwd=src_dir  # Run from script directory
                )
                
                # Check for output file in possible locations
                time.sleep(1)  # Give a moment for file system to update
                
                # Check multiple potential output locations
                possible_paths = [
                    os.path.join(output_dir, f"{base_filename}_forward_out.txt"),
                    os.path.join(script_dir, "outputfiles", f"{base_filename}_forward_out.txt"),
                    os.path.join(os.path.dirname(script_dir), "outputfiles", f"{base_filename}_forward_out.txt")
                ]
                
                found_path = None
                for path in possible_paths:
                    if os.path.exists(path):
                        found_path = path
                        break
                
                if found_path:
                    # If found but not in the desired location, copy it there
                    if found_path != os.path.join(output_dir, f"{base_filename}_forward_out.txt"):
                        import shutil
                        target_path = os.path.join(output_dir, f"{base_filename}_forward_out.txt")
                        shutil.copy2(found_path, target_path)
                        print(f"Found output at {found_path}, copied to {target_path}")
                    else:
                        print(f"Successfully processed {filename} with forward, output at {found_path}")
                else:
                    print(f"Warning: Could not find output file for {filename} with forward")
                    print(f"Command output: {forward_output.stdout}")
                    print(f"Command errors: {forward_output.stderr}")
            except subprocess.CalledProcessError as e:
                print(f"Error processing {filename} with forward: {e}")
                continue
            
            # Run noforward executable with similar logic
            try:
                print(f"Processing {filename} with noforward...")
                env = os.environ.copy()
                env["OUTPUT_DIR"] = output_dir
                
                noforward_output = subprocess.run(
                    [noforward_exe, input_file_path, '50'], 
                    capture_output=True, 
                    text=True, 
                    env=env,
                    cwd=src_dir  # Change cwd here as well
                )
                
                time.sleep(0.1)
                
                possible_paths = [
                    os.path.join(output_dir, f"{base_filename}_no_forward_out.txt"),
                    os.path.join(script_dir, "outputfiles", f"{base_filename}_no_forward_out.txt"),
                    os.path.join(os.path.dirname(script_dir), "outputfiles", f"{base_filename}_no_forward_out.txt")
                ]
                
                found_path = None
                for path in possible_paths:
                    if os.path.exists(path):
                        found_path = path
                        break
                
                if found_path:
                    if found_path != os.path.join(output_dir, f"{base_filename}_no_forward_out.txt"):
                        import shutil
                        target_path = os.path.join(output_dir, f"{base_filename}_no_forward_out.txt")
                        shutil.copy2(found_path, target_path)
                        print(f"Found output at {found_path}, copied to {target_path}")
                    else:
                        print(f"Successfully processed {filename} with noforward, output at {found_path}")
                else:
                    print(f"Warning: Could not find output file for {filename} with noforward")
                    print(f"Command output: {noforward_output.stdout}")
                    print(f"Command errors: {noforward_output.stderr}")
            except subprocess.CalledProcessError as e:
                print(f"Error processing {filename} with noforward: {e}")
                continue

def main():
    # Set directories - adjust these paths as needed
    global script_dir
    script_dir = os.path.dirname(os.path.abspath(__file__))
    input_dir = os.path.join(script_dir, 'inputfiles')
    src_dir = os.path.join(script_dir, 'src')
    output_dir = os.path.join(script_dir, 'outputfiles')
    
    process_input_files(input_dir, src_dir, output_dir)

if __name__ == "__main__":
    main()
