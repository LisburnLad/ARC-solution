import os
import zipfile

def zip_project(output_filename):
    # Define the files and directories to include in the zip file
    files_to_include = ['Makefile', 'safe_run.py', 'version.txt']
    directories_to_include = ['src', 'headers']

    # Create a ZipFile object
    with zipfile.ZipFile(output_filename, 'w', zipfile.ZIP_DEFLATED) as zipf:
        # Add files to the zip file
        for file in files_to_include:
            if os.path.exists(file):
                zipf.write(file)

        # Add directories to the zip file
        for directory in directories_to_include:
            for root, _, files in os.walk(directory):
                for file in files:
                    file_path = os.path.join(root, file)
                    zipf.write(file_path)

# Call the function to create the zip file
zip_project('arc-agi.zip')