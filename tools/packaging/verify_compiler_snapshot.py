#!/usr/bin/env python3
"""Verify codeC's pinned compiler inputs, including metadata-free producer copies."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

INPUT_DIRS = ('src', 'include', 'third_party', 'make')

def verify(root, manifest, pin):
    root = Path(root).resolve()
    record = json.loads(Path(manifest).read_text())
    if record.get('commit') != pin or not record.get('files'):
        raise ValueError('compiler manifest revision differs from source pin')
    files = record['files']
    for name, expected in files.items():
        rel = Path(name)
        if rel.is_absolute() or '..' in rel.parts or not rel.parts:
            raise ValueError('unsafe compiler manifest path')
        p = root / rel
        if any(x.is_symlink() for x in (p, *p.parents)) or not p.is_file():
            raise ValueError('compiler input absent or symlink: ' + name)
        if hashlib.sha256(p.read_bytes()).hexdigest() != expected['sha256'] or p.stat().st_mode & 0o777 != expected['mode']:
            raise ValueError('compiler input drift: ' + name)
    # Additional build inputs must not be silently admitted by a frozen copy.
    actual = {str(p.relative_to(root)) for directory in INPUT_DIRS
              for p in (root / directory).rglob('*')
              if p.name != '.DS_Store' and (p.is_file() or p.is_symlink())}
    expected = {n for n in files if Path(n).parts[0] in INPUT_DIRS}
    if actual != expected:
        raise ValueError('compiler input set differs from manifest')
    git = subprocess.run(['git', '-C', str(root), 'rev-parse', '--show-toplevel'], capture_output=True, text=True)
    if git.returncode == 0 and Path(git.stdout.strip()).resolve() == root:
        head = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
        if head != pin:
            raise ValueError('Compiler source revision drift')
        dirty = subprocess.check_output(['git', '-C', str(root), 'status', '--porcelain', '--', 'Makefile', 'VERSION', *INPUT_DIRS])
        if dirty:
            raise ValueError('compiler build inputs differ from committed source')
    return pin

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', required=True)
    parser.add_argument('--manifest', default='make/fisics-snapshot-inputs.json')
    parser.add_argument('--pin-file', default='make/fisics-source-pin.txt')
    args = parser.parse_args()
    try:
        print(verify(args.root, args.manifest, Path(args.pin_file).read_text().strip()))
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')
