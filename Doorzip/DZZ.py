import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import os
import dzi_core

class DZZFileManager:
    def __init__(self, root):
        self.root = root
        self.root.title("DZZ - Quick Target Compressor")
        self.root.geometry("750x450")

        toolbar = ttk.Frame(root, padding=5)
        toolbar.pack(side=tk.TOP, fill=tk.X)
        self.btn_browse = ttk.Button(toolbar, text="📂 Browse Directory", command=self.load_directory)
        self.btn_browse.pack(side=tk.LEFT, padx=2)

        path_frame = ttk.Frame(root, padding=5)
        path_frame.pack(side=tk.TOP, fill=tk.X)
        ttk.Label(path_frame, text="Path:").pack(side=tk.LEFT, padx=2)
        self.path_var = tk.StringVar(value=os.path.expanduser("~"))
        self.path_entry = ttk.Entry(path_frame, textvariable=self.path_var)
        self.path_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
        self.path_entry.bind("<Return>", lambda e: self.load_directory())

        columns = ("name", "size", "type")
        self.tree = ttk.Treeview(root, columns=columns, show="headings")
        self.tree.heading("name", text="Name", anchor=tk.W)
        self.tree.heading("size", text="Size", anchor=tk.E)
        self.tree.heading("type", text="Type", anchor=tk.W)
        self.tree.column("name", width=380, anchor=tk.W)
        self.tree.column("size", width=120, anchor=tk.E)
        self.tree.column("type", width=150, anchor=tk.W)
        
        scrollbar = ttk.Scrollbar(root, orient=tk.VERTICAL, command=self.tree.yview)
        self.tree.configure(yscrollcommand=scrollbar.set)
        self.tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.tree.bind("<Double-1>", self.on_double_click)

        bottom_frame = ttk.Frame(root, padding=10)
        bottom_frame.pack(side=tk.BOTTOM, fill=tk.X)
        ttk.Label(bottom_frame, text="Target Bytes:").pack(side=tk.LEFT, padx=5)
        self.entry_bytes = ttk.Entry(bottom_frame, width=20)
        self.entry_bytes.pack(side=tk.LEFT, padx=5)
        self.btn_compress = ttk.Button(bottom_frame, text="⚡ Quick Compress (.dzz)", command=self.quick_compress)
        self.btn_compress.pack(side=tk.RIGHT, padx=5)

        self.load_directory()

    def load_directory(self):
        for item in self.tree.get_children():
            self.tree.delete(item)
        current_path = self.path_var.get()
        if not os.path.exists(current_path): return
        try:
            parent = os.path.dirname(current_path)
            if parent != current_path:
                self.tree.insert("", tk.END, values=("[..]", "", "Parent Directory"))
            for entry in os.listdir(current_path):
                fp = os.path.join(current_path, entry)
                if os.path.isdir(fp):
                    self.tree.insert("", tk.END, values=(entry, "", "Folder"))
                else:
                    self.tree.insert("", tk.END, values=(entry, f"{os.path.getsize(fp):,} bytes", "File"))
        except PermissionError:
            pass

    def on_double_click(self, event):
        sel = self.tree.selection()
        if not sel: return
        vals = self.tree.item(sel, "values")
        if vals[0] == "[..]":
            self.path_var.set(os.path.dirname(self.path_var.get()))
            self.load_directory()
        elif vals[2] == "Folder":
            self.path_var.set(os.path.join(self.path_var.get(), vals[0]))
            self.load_directory()

    def quick_compress(self):
        sel = self.tree.selection()
        if not sel: return
        vals = self.tree.item(sel, "values")
        if vals[0] == "[..]": return
        if not self.entry_bytes.get().strip().isdigit():
            messagebox.showerror("Error", "Enter valid bytes!")
            return
        target_path = os.path.join(self.path_var.get(), vals[0])
        out_path = target_path + ".dzz"
        if dzi_core.target_byte_compress(target_path, out_path, int(self.entry_bytes.get().strip())):
            messagebox.showinfo("Success", f"Saved to: {out_path}")
            self.load_directory()

if __name__ == "__main__":
    root = tk.Tk()
    app = DZZFileManager(root)
    root.mainloop()