import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import os

try:
    import dzi_core
except ImportError:
    import sys
    sys.path.append(os.path.dirname(__file__))
    import dzi_core

class DZExtractApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Doorzip - Dedicated Extractor")
        self.root.geometry("600x300")
        self.root.resizable(False, False)

        # Main Frame
        frame = ttk.Frame(root, padding=15)
        frame.pack(fill=tk.BOTH, expand=True)

        ttk.Label(frame, text="Doorzip Archive Extractor", font=("Arial", 12, "bold")).pack(pady=5)
        ttk.Label(frame, text="Select a .DZi, .ZD, or .DZZ archive to extract:", font=("Arial", 9)).pack(pady=5)

        # File selection row
        file_frame = ttk.Frame(frame)
        file_frame.pack(fill=tk.X, pady=10)

        self.path_var = tk.StringVar()
        self.entry_path = ttk.Entry(file_frame, textvariable=self.path_var, width=50)
        self.entry_path.pack(side=tk.LEFT, padx=5)

        self.btn_browse = ttk.Button(file_frame, text="📂 Browse...", command=self.browse_archive)
        self.btn_browse.pack(side=tk.LEFT, padx=5)

        # Action button
        self.btn_extract = ttk.Button(frame, text="📦 Extract Archive Now", command=self.execute_extraction)
        self.btn_extract.pack(pady=20)

    def browse_archive(self):
        filename = filedialog.askopenfilename(
            title="Select Doorzip Archive",
            filetypes=[
                ("Doorzip Archives", "*.dzi *.zd *.dzz"),
                ("All Files", "*.*")
            ]
        )
        if filename:
            self.path_var.set(filename)

    def execute_extraction(self):
        archive_path = self.path_var.get().strip()
        if not archive_path or not os.path.exists(archive_path):
            messagebox.showerror("Error", "Please select a valid archive file first!")
            return

        # Default extraction folder is right next to the file, named after it without extension
        extract_dir = os.path.splitext(archive_path)[0]
        os.makedirs(extract_dir, exist_ok=True)

        # Use your existing core logic from dzi_core.py
        success = dzi_core.extract_dzi(archive_path, extract_dir)

        if success:
            messagebox.showinfo("Success", f"Successfully extracted!\n\nFolder: {extract_dir}")
            self.path_var.set("")
        else:
            messagebox.showerror("Error", "Extraction failed. Invalid container or padding error.")

if __name__ == "__main__":
    root = tk.Tk()
    app = DZExtractApp(root)
    root.mainloop()