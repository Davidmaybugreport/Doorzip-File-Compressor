import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import os

try:
    import dzi_core
except ImportError:
    import sys
    sys.path.append(os.path.dirname(__file__))
    import dzi_core

class DziFileManager:
    def __init__(self, root):
        self.root = root
        self.root.title("ZDI - Flagship File Manager")
        self.root.geometry("800x500")

        style = ttk.Style()
        style.theme_use("vista" if "vista" in style.theme_names() else "default")

        toolbar = ttk.Frame(root, padding=5)
        toolbar.pack(side=tk.TOP, fill=tk.X)

        self.btn_add = ttk.Button(toolbar, text="📂 Add/Open", command=self.browse_file)
        self.btn_add.pack(side=tk.LEFT, padx=2)

        self.btn_compress = ttk.Button(toolbar, text="📦 Target Byte Compress (.dzi)", command=self.open_compress_modal)
        self.btn_compress.pack(side=tk.LEFT, padx=2)

        self.btn_refresh = ttk.Button(toolbar, text="🔄 Refresh", command=self.load_directory)
        self.btn_refresh.pack(side=tk.LEFT, padx=2)

        path_frame = ttk.Frame(root, padding=5)
        path_frame.pack(side=tk.TOP, fill=tk.X)
        ttk.Label(path_frame, text="Path:").pack(side=tk.LEFT, padx=2)
        self.path_var = tk.StringVar(value=os.path.expanduser("~"))
        self.path_entry = ttk.Entry(path_frame, textvariable=self.path_var)
        self.path_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
        self.path_entry.bind("<Return>", lambda e: self.load_directory())

        columns = ("name", "size", "type")
        self.tree = ttk.Treeview(root, columns=columns, show="headings", selectmode="browse")
        self.tree.heading("name", text="Name", anchor=tk.W)
        self.tree.heading("size", text="Size", anchor=tk.E)
        self.tree.heading("type", text="Type", anchor=tk.W)
        self.tree.column("name", width=400, anchor=tk.W)
        self.tree.column("size", width=120, anchor=tk.E)
        self.tree.column("type", width=150, anchor=tk.W)

        scrollbar = ttk.Scrollbar(root, orient=tk.VERTICAL, command=self.tree.yview)
        self.tree.configure(yscrollcommand=scrollbar.set)
        self.tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.tree.bind("<Double-1>", self.on_item_double_click)
        self.load_directory()

    def load_directory(self):
        for item in self.tree.get_children():
            self.tree.delete(item)
        current_path = self.path_var.get()
        if not os.path.exists(current_path): return
        try:
            parent_dir = os.path.dirname(current_path)
            if parent_dir != current_path:
                self.tree.insert("", tk.END, values=("[..]", "", "Parent Directory"))
            for entry in os.listdir(current_path):
                full_path = os.path.join(current_path, entry)
                if os.path.isdir(full_path):
                    self.tree.insert("", tk.END, values=(entry, "", "Folder"))
                else:
                    self.tree.insert("", tk.END, values=(entry, f"{os.path.getsize(full_path):,} bytes", "File"))
        except PermissionError:
            pass

    def on_item_double_click(self, event):
        sel = self.tree.selection()
        if not sel: return
        vals = self.tree.item(sel, "values")
        if vals[0] == "[..]":
            self.path_var.set(os.path.dirname(self.path_var.get()))
            self.load_directory()
        elif vals[2] == "Folder":
            self.path_var.set(os.path.join(self.path_var.get(), vals[0]))
            self.load_directory()

    def browse_file(self):
        filename = filedialog.askopenfilename()
        if filename:
            self.path_var.set(os.path.dirname(filename))
            self.load_directory()

    def open_compress_modal(self):
        sel = self.tree.selection()
        if not sel:
            messagebox.showwarning("Warning", "Select a file or folder first!")
            return
        vals = self.tree.item(sel, "values")
        if vals[0] == "[..]": return

        target_path = os.path.join(self.path_var.get(), vals[0])
        modal = tk.Toplevel(self.root)
        modal.title("ZDI - Byte Target Compression")
        modal.geometry("400x200")
        modal.resizable(False, False)

        ttk.Label(modal, text=f"Target: {vals[0]}", font=("Arial", 10, "bold")).pack(pady=10)
        ttk.Label(modal, text="Enter Exact Target Size (Bytes):").pack(pady=5)
        entry_target = ttk.Entry(modal, width=30)
        entry_target.pack(pady=5)

        def execute_compression():
            if not entry_target.get().strip().isdigit():
                messagebox.showerror("Error", "Enter valid bytes!")
                return
            output_file = target_path + ".dzi"
            if dzi_core.target_byte_compress(target_path, output_file, int(entry_target.get().strip())):
                messagebox.showinfo("Success", f"Saved as: {output_file}")
                modal.destroy()
                self.load_directory()
            else:
                messagebox.showerror("Error", "Target size smaller than minimum payload.")

        ttk.Button(modal, text="Compress Now", command=execute_compression).pack(pady=15)

if __name__ == "__main__":
    root = tk.Tk()
    app = DziFileManager(root)
    root.mainloop()