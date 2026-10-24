import sys
import os
import tkinter as tk
from tkinter import messagebox

try:
    import dzi_core
except ImportError:
    sys.path.append(os.path.dirname(__file__))
    import dzi_core

if len(sys.argv) < 2:
    sys.exit(1)

target_file = sys.argv[1]
extract_dir = os.path.splitext(target_file)[0]  # Folder name matches the archive name

# Create the target extraction directory
os.makedirs(extract_dir, exist_ok=True)

# Run the custom unpadding extraction engine
success = dzi_core.extract_dzi(target_file, extract_dir)

# Show a clean, native notification popup
root = tk.Tk()
root.withdraw()  # Hide the main tkinter window shell

if success:
    messagebox.showinfo("Doorzip Extractor", f"Successfully extracted!\n\nFolder: {os.path.basename(extract_dir)}")
else:
    messagebox.showerror("Doorzip Extractor", "Extraction failed. Invalid container or padding error.")