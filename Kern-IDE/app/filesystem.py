from __future__ import annotations

from pathlib import Path
from tkinter import ttk


class FileExplorer:
    def __init__(self, tree: ttk.Treeview, root_path: Path) -> None:
        self.tree = tree
        self.root_path = root_path
        self._node_to_path: dict[str, Path] = {}
        self._loaded_dirs: set[str] = set()
        self._placeholder_text = "..."

    def set_root(self, root_path: Path) -> None:
        self.root_path = root_path
        self.refresh()

    def path_for_node(self, node_id: str) -> Path | None:
        return self._node_to_path.get(node_id)

    def refresh(self) -> None:
        self.tree.delete(*self.tree.get_children(""))
        self._node_to_path.clear()
        self._loaded_dirs.clear()
        root = self.tree.insert("", "end", text=self.root_path.name, open=True)
        self._node_to_path[root] = self.root_path
        self._loaded_dirs.add(root)
        # Eager-load only the first level; subdirectories are lazy-loaded on expand.
        self._load_children(root, self.root_path)

    def _load_children(self, parent_node: str, path: Path) -> None:
        try:
            items = sorted(path.iterdir(), key=lambda p: (p.is_file(), p.name.lower()))
        except Exception:
            return
        for item in items:
            if item.name.startswith(".git") or item.name in {"build", "dist", "__pycache__", ".venv", "node_modules"}:
                continue
            node = self.tree.insert(parent_node, "end", text=item.name, open=False)
            self._node_to_path[node] = item
            if item.is_dir():
                # Insert a placeholder so the directory can be expanded.
                self.tree.insert(node, "end", text=self._placeholder_text, open=False)

    def on_tree_open(self, _event: object = None) -> None:
        """
        Lazy-load directory children on expansion (`<<TreeviewOpen>>`).

        We load only immediate children and replace the placeholder child.
        """
        node_id = self.tree.focus() or (self.tree.selection()[0] if self.tree.selection() else "")
        if not node_id or node_id in self._loaded_dirs:
            return

        path = self.path_for_node(node_id)
        if not path or not path.is_dir():
            return

        # Remove placeholders before re-populating.
        for child in list(self.tree.get_children(node_id)):
            if self.tree.item(child, "text") == self._placeholder_text:
                self.tree.delete(child)

        self._load_children(node_id, path)
        self._loaded_dirs.add(node_id)

