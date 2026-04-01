from __future__ import annotations

from pathlib import Path
from tkinter import ttk


class FileExplorer:
    def __init__(self, tree: ttk.Treeview, root_path: Path) -> None:
        self.tree = tree
        self.root_path = root_path
        self._node_to_path: dict[str, Path] = {}

    def set_root(self, root_path: Path) -> None:
        self.root_path = root_path
        self.refresh()

    def path_for_node(self, node_id: str) -> Path | None:
        return self._node_to_path.get(node_id)

    def refresh(self) -> None:
        self.tree.delete(*self.tree.get_children(""))
        self._node_to_path.clear()
        root = self.tree.insert("", "end", text=self.root_path.name, open=True)
        self._node_to_path[root] = self.root_path
        self._add_children(root, self.root_path)

    def _add_children(self, parent_node: str, path: Path) -> None:
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
                self._add_children(node, item)

