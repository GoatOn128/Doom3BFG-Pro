#!/usr/bin/env python3
"""Extract errors and warnings from build.log into a digest fragment.

The status file is written first so the publish job always has data,
even if log parsing breaks. Any script error is captured into the
fragment itself so it stays visible in build-digest.md.

Warnings are grouped by kind: the clang warning flag or MSVC
diagnostic code found anywhere in the line. The flag is searched
anywhere because parallel builds interleave output lines in
build.log, so a strict "location: message [flag]" pattern misses
most of them. Kinds are sorted by frequency and each keeps a list
of distinct example lines.
"""

import os
import re
import traceback
from collections import OrderedDict

LEG = os.environ.get("LEG", "unknown")
STATUS = os.environ.get("STATUS", "unknown")

ERROR_RE = re.compile(
    "error:|fatal error:|error C[0-9]+|Error C[0-9]+|error LNK[0-9]+|"
    "error MSB[0-9]+|CMake Error|undefined reference|undefined symbol|"
    "Undefined symbols|duplicate symbol|symbol\\(s\\) not found|"
    "unresolved external|no such file|cannot open|LLVM ERROR|Stack dump:|"
    ">>> referenced|referenced from|UNCRUSTIFY_FAILED|Build FAILED"
)
WARNING_RE = re.compile("warning:|warning C[0-9]+")
FLAG_RE = re.compile(r"\[-W([-\w]+)\]")
MSVC_CODE_RE = re.compile(r"warning C([0-9]+):")

MAX_ERROR_LINES = 300
MAX_KINDS = 300
MAX_EXAMPLES = 400


def warn_key(line):
    """Return a stable kind key: warning flag or MSVC code when
    present, else a prefix of the message."""
    m = FLAG_RE.search(line)
    if m:
        return "-" + m.group(1)
    m = MSVC_CODE_RE.search(line)
    if m:
        return "C" + m.group(1)
    m = re.search(r"warning: (.{0,100})", line)
    if m:
        return m.group(1)
    return line[:100]


def main():
    with open("status-" + LEG, "w") as f:
        f.write(LEG + "|" + STATUS + "\n")

    with open("digest-fragment-" + LEG + ".md", "w") as f:
        f.write("## " + LEG + "\n\nstatus: " + STATUS + "\n\n")
        try:
            lines = []
            if os.path.exists("build.log"):
                with open("build.log", errors="replace") as fh:
                    lines = fh.read().splitlines()
            else:
                lines = ["no build.log: the job failed before or during CMake setup, before the build produced output"]
            errors = [l for l in lines if ERROR_RE.search(l)]
            warnings = [l for l in lines if WARNING_RE.search(l)]

            # identical error lines repeat (headers per job, retries),
            # keep one of each
            seen_errors = OrderedDict()
            for l in errors:
                seen_errors.setdefault(l.strip()[:400], None)

            groups = OrderedDict()
            for l in warnings:
                groups.setdefault(warn_key(l.strip()), []).append(l.strip()[:400])

            # most frequent kinds first so the big problems lead
            ordered = sorted(groups.items(), key=lambda kv: -len(kv[1]))

            f.write("errors: " + str(len(seen_errors)) + "\n")
            f.write("warnings: " + str(len(warnings)) + " in " + str(len(groups)) + " kinds\n\n")

            f.write("### errors\n\n")
            for l in list(seen_errors.keys())[:MAX_ERROR_LINES]:
                f.write(l + "\n")

            # raw lines around each failing build target: failures that
            # print no matching error line (crashes, out of memory)
            # stay visible in the digest this way
            f.write("\n### failure context\n\n")
            target_re = re.compile(r"(CMake Error at [^:]+|FAILED: .+|.+ : error (?:C|LNK|MSB)[0-9]+)")
            ctx = OrderedDict()
            seen_file = set()
            for i, l in enumerate(lines):
                # anchor on every distinct error line, not just the known
                # target patterns: cmake message bodies and linker symbol
                # lists are printed right around the matching line.
                # one anchor per source file so a wall of errors from one
                # file cannot crowd out the linker and cmake chunks
                if target_re.search(l) or ERROR_RE.search(l):
                    loc = re.search(r"([A-Za-z0-9_.-]+\.(?:cpp|h|c|cc|mm|m))", l)
                    key = loc.group(1) if loc else l.strip()[:120]
                    if key in seen_file:
                        continue
                    seen_file.add(key)
                    target = l.strip()[:120]
                    if target and target not in ctx:
                        if "Undefined symbols" in l:
                            # the referenced-from entries are below the
                            # headline, keep room for them
                            ctx[target] = lines[max(0, i - 2):i + 30]
                        else:
                            ctx[target] = lines[max(0, i - 8):i + 6]
            shown = 0
            for target, chunk in ctx.items():
                if shown >= 10:
                    break
                f.write("- " + target + "\n\n```\n" +
                        "\n".join(c[:400] for c in chunk) + "\n```\n\n")
                shown += 1

            f.write("\n### warning kinds\n\n")
            for key, examples in ordered[:MAX_KINDS]:
                f.write("- [" + str(len(examples)) + "x] " + key + "\n")
                seen = set()
                shown = 0
                for e in examples:
                    if e in seen:
                        continue
                    seen.add(e)
                    f.write("  - " + e + "\n")
                    shown += 1
                    if shown >= MAX_EXAMPLES:
                        break
        except Exception:
            f.write("digest script error:\n\n```\n" + traceback.format_exc() + "\n```\n")

    if os.path.exists("build.log"):
        with open("build.log", errors="replace") as src, open("log-" + LEG + ".log", "w") as dst:
            dst.write(src.read())


main()
