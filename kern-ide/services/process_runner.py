from __future__ import annotations

import subprocess
import threading
from collections.abc import Callable
from pathlib import Path


OutputCallback = Callable[[str, str], None]
DoneCallback = Callable[[int], None]
ErrorCallback = Callable[[str], None]


class StreamingProcessRunner:
    def __init__(self) -> None:
        self.process: subprocess.Popen[str] | None = None
        self.thread: threading.Thread | None = None
        self._lock = threading.Lock()

    def is_running(self) -> bool:
        with self._lock:
            return self.process is not None and self.process.poll() is None

    def stop(self) -> None:
        with self._lock:
            proc = self.process
        if proc is None or proc.poll() is not None:
            return
        try:
            proc.terminate()
        except Exception:
            try:
                proc.kill()
            except Exception:
                return

    def run(
        self,
        command: list[str],
        cwd: str | Path,
        on_output: OutputCallback,
        on_done: DoneCallback,
        on_error: ErrorCallback,
        env: dict[str, str] | None = None,
    ) -> bool:
        if self.is_running():
            return False

        run_cwd = str(cwd)

        def worker() -> None:
            code = -1
            try:
                popen_kw: dict = {
                    "cwd": run_cwd,
                    "stdout": subprocess.PIPE,
                    "stderr": subprocess.STDOUT,
                    "text": True,
                    "encoding": "utf-8",
                    "errors": "replace",
                    "bufsize": 1,
                }
                if env is not None:
                    popen_kw["env"] = env
                proc = subprocess.Popen(command, **popen_kw)
                with self._lock:
                    self.process = proc
                out = proc.stdout
                if out is not None:
                    for line in iter(out.readline, ""):
                        on_output("stdout", line)
                proc.wait()
                code = int(proc.returncode or 0)
            except FileNotFoundError:
                exe = command[0] if command else "(no command)"
                on_error(f"Executable not found: {exe!r}\n")
            except PermissionError as exc:
                exe = command[0] if command else "(no command)"
                on_error(f"Permission denied when running {exe!r}: {exc}\n")
            except OSError as exc:
                on_error(f"Could not start process ({type(exc).__name__}): {exc}\n")
            except Exception as exc:  # noqa: BLE001
                on_error(f"Process execution failed: {type(exc).__name__}: {exc}\n")
            finally:
                with self._lock:
                    self.process = None
                on_done(code)

        self.thread = threading.Thread(target=worker, daemon=True)
        self.thread.start()
        return True
