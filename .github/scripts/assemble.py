#!/usr/bin/env python3
"""Merge per-leg logs and digest fragments into per-platform build logs
and build-digest.md. Legs are grouped by their platform prefix
(windows/linux/macos); legs with no build log stay in the artifacts."""

import glob
import os

MAX_LOG = 400000  # bytes kept per failed leg

PLATFORMS = {
    "windows": "build-windows.log",
    "linux": "build-linux.log",
    "macos": "build-macos.log",
}


def read(path):
    with open(path, errors="replace") as f:
        return f.read()


def main():
    frag_files = sorted(glob.glob("logs/digest-fragment-*.md"))
    status_files = sorted(glob.glob("logs/status-*"))
    log_files = sorted(glob.glob("logs/log-*.log"))

    statuses = {}
    for sf in status_files:
        parts = read(sf).strip().split("|", 1)
        if len(parts) == 2:
            statuses[parts[0]] = parts[1]

    digest = ["# Build digest\n\n"]
    # publish runs on a workflow_run event where GITHUB_SHA points at the
    # default branch head, not the built commit: COMMIT_SHA wins when set
    commit = os.environ.get("COMMIT_SHA") or os.environ.get("GITHUB_SHA", "unknown")
    digest.append("commit: %s\n\n" % commit[:12])
    for leg in sorted(statuses):
        digest.append("- %s: %s\n" % (leg, statuses[leg]))
    digest.append("\n")
    for ff in frag_files:
        digest.append(read(ff).strip() + "\n\n")
    with open("build-digest.md", "w") as f:
        f.write("".join(digest))

    # group the failed leg logs by runner platform; legs with another
    # prefix (uncrustify) carry no build log and stay in the artifacts
    by_platform = {name: [] for name in PLATFORMS}
    for lf in log_files:
        leg = os.path.basename(lf)[len("log-"):-len(".log")]
        if statuses.get(leg) == "success":
            # successful leg logs stay in the artifacts only
            continue
        for prefix, out in PLATFORMS.items():
            if leg.startswith(prefix):
                text = read(lf)
                if len(text) > MAX_LOG:
                    half = MAX_LOG // 2
                    text = text[:half] + "\n... [truncated] ...\n" + text[-half:]
                by_platform[prefix].append("==== %s (%s) ====\n\n" % (leg, statuses.get(leg, "unknown")))
                by_platform[prefix].append(text + "\n\n")
                break

    for prefix, out in PLATFORMS.items():
        legs = [l for l in sorted(statuses) if l.startswith(prefix)]
        if by_platform[prefix]:
            content = "".join(by_platform[prefix])
        elif legs:
            content = "all %s legs succeeded\n" % prefix
        else:
            content = "no %s legs ran\n" % prefix
        with open(PLATFORMS[prefix], "w") as f:
            f.write(content)


main()
