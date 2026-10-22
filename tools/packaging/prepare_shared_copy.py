#!/usr/bin/env python3
"""Create an isolated copy of declared vendored inputs for legacy builders."""
import argparse
from pathlib import Path
import shutil
import subprocess


def prepare(source, output):
    source = Path(source).resolve()
    output = Path(output).absolute()
    if output == source or output.is_relative_to(source):
        raise ValueError("output must be outside the shared source")
    if any(p.is_symlink() for p in (output, *output.parents)):
        raise ValueError("output must not traverse symlinks")
    repo = Path(subprocess.check_output(["git", "-C", str(source), "rev-parse", "--show-toplevel"], text=True).strip())
    names = subprocess.check_output(["git", "-C", str(repo), "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--", str(source)]).split(b"\0")
    files = []
    for raw in names:
        if not raw:
            continue
        path = repo / raw.decode()
        if path.is_symlink() or not path.is_file():
            raise ValueError("shared inputs must be ordinary files")
        files.append((path, path.relative_to(source)))
    if not files:
        raise ValueError("shared inputs are empty")
    output.mkdir(parents=True, exist_ok=False)
    for path, relative in files:
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    return len(files)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    print("copied", prepare(args.source, args.output), "declared shared inputs")
