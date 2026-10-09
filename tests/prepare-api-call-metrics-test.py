"""Extract production trampoline builders and BSON emitter for a standalone test.

Run from the repository root with Python, then compile api-call-metrics.c using
MSVC (see tests/README-api-call-metrics.md). No sandbox or DLL injection needed.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "objects" / "metrics-tests"
OUT.mkdir(parents=True, exist_ok=True)


def function(source, signature):
    start = source.index(signature)
    brace = source.index("\n{", start) + 1
    # These functions have no unbalanced braces in comments/strings.
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if not depth:
                return source[start:pos + 1] + "\n"
    raise ValueError(signature)


for bits in (32, 64):
    source = (ROOT / f"hooking_{bits}.c").read_text(encoding="utf-8")
    content = "\n".join(function(source, f"static void {name}(hook_t *h)")
                        for name in ("hook_create_pre_tramp", "hook_create_pre_tramp_notail"))
    (OUT / f"tramp-{bits}.h").write_text(content, encoding="utf-8")

source = (ROOT / "log.c").read_text(encoding="utf-8")
(OUT / "metrics-bson.h").write_text(
    function(source, "static void log_api_call_metrics("), encoding="utf-8")
