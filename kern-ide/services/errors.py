"""Format exceptions for the IDE output panel and short user-facing messages."""

from __future__ import annotations

import traceback

# cap traceback size so the output buffer stays responsive.
MAX_TRACEBACK_CHARS = 6000


def format_exception_brief(exc: BaseException) -> str:
    return f"{type(exc).__name__}: {exc}"


def format_exception_block(context: str, exc: BaseException) -> str:
    """Full traceback text for logging to the output panel."""
    head = f"[{context}] {format_exception_brief(exc)}"
    tb = "".join(traceback.format_exception(type(exc), exc, exc.__traceback__))
    if len(tb) > MAX_TRACEBACK_CHARS:
        tb = tb[: MAX_TRACEBACK_CHARS - 24] + "\n... (traceback truncated)\n"
    return head + "\n" + tb
