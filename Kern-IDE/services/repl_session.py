from __future__ import annotations

import subprocess
import threading
from collections.abc import Callable
from pathlib import Path


OutputCallback = Callable[[str, str], None]
StateCallback = Callable[[str], None]


class ReplSession:
    def __init__(self, on_output: OutputCallback, on_state: StateCallback) -> None:
        self._on_output = on_output
        self._on_state = on_state
        self._proc: subprocess.Popen[str] | None = None
        self._reader_thread: threading.Thread | None = None
        self._lock = threading.Lock()
        self._cwd: str | None = None

    def is_alive(self) -> bool:
        with self._lock:
            return self._proc is not None and self._proc.poll() is None

    def start(self, repl_exe: str, cwd: str | Path, env: dict[str, str] | None = None) -> bool:
        run_cwd = str(cwd)
        with self._lock:
            if self._proc is not None and self._proc.poll() is None and self._cwd == run_cwd:
                return True
        self.stop()
        try:
            popen_kw: dict = {
                "cwd": run_cwd,
                "stdin": subprocess.PIPE,
                "stdout": subprocess.PIPE,
                "stderr": subprocess.STDOUT,
                "text": True,
                "encoding": "utf-8",
                "errors": "replace",
                "bufsize": 1,
            }
            if env is not None:
                popen_kw["env"] = env
            proc = subprocess.Popen([repl_exe], **popen_kw)
        except FileNotFoundError:
            self._on_output("stderr", f"REPL executable not found: {repl_exe!r}\n")
            self._on_state("stopped")
            return False
        except PermissionError as exc:
            self._on_output("stderr", f"Permission denied starting REPL ({repl_exe!r}): {exc}\n")
            self._on_state("stopped")
            return False
        except OSError as exc:
            self._on_output("stderr", f"Could not start REPL ({type(exc).__name__}): {exc}\n")
            self._on_state("stopped")
            return False
        except Exception as exc:  # noqa: BLE001
            self._on_output("stderr", f"Failed to start REPL process: {type(exc).__name__}: {exc}\n")
            self._on_state("stopped")
            return False

        with self._lock:
            self._proc = proc
            self._cwd = run_cwd
        self._reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self._reader_thread.start()
        self._on_state("running")
        return True

    def send(self, command: str) -> bool:
        if not command.strip():
            return True
        with self._lock:
            proc = self._proc
        if proc is None or proc.poll() is not None or proc.stdin is None:
            self._on_state("stopped")
            return False
        try:
            proc.stdin.write(command + "\n")
            proc.stdin.flush()
            return True
        except Exception as exc:  # noqa: BLE001
            self._on_output("stderr", f"REPL send failed: {exc}\n")
            self._on_state("stopped")
            return False

    def stop(self) -> None:
        with self._lock:
            proc = self._proc
        if proc is None:
            self._on_state("stopped")
            return
        try:
            if proc.stdin is not None:
                try:
                    proc.stdin.write("exit\n")
                    proc.stdin.flush()
                except Exception:
                    pass
            proc.terminate()
        except Exception:
            try:
                proc.kill()
            except Exception:
                pass
        finally:
            with self._lock:
                self._proc = None
            self._on_state("stopped")

    def _reader_loop(self) -> None:
        with self._lock:
            proc = self._proc
        if proc is None:
            self._on_state("stopped")
            return
        try:
            out = proc.stdout
            if out is not None:
                for line in iter(out.readline, ""):
                    self._on_output("stdout", line)
            proc.wait()
            self._on_output("system", f"[repl exit={proc.returncode if proc.returncode is not None else -1}]\n")
        except Exception as exc:  # noqa: BLE001
            self._on_output("stderr", f"REPL reader failed: {exc}\n")
        finally:
            with self._lock:
                self._proc = None
            self._on_state("stopped")
