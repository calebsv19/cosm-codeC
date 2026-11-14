#!/usr/bin/env python3
"""Compile with fisiCs' actual include graph and publish Make dependencies."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def make_path(value):
    if not isinstance(value, str) or not value or any(c in value for c in '\n\r\0:\\%|;*?[]'):
        raise ValueError('invalid dependency path')
    return (value.replace('$', '$$').replace('#', '\\#')
            .replace(' ', '\\ ').replace('\t', '\\\t'))


def compile_command(command):
    # Link commands keep the existing compiler driver and options.
    if '-c' not in command:
        return subprocess.call(command)
    output_index = command.index('-o') + 1
    output = Path(command[output_index])
    source = command[command.index('-c') + 1]
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.fisics-', dir=output.parent) as tmp:
        stage = Path(tmp)
        obj, graph, dep = stage/'object.o', stage/'includes.json', stage/'object.d'
        args = list(command)
        args[output_index] = str(obj)
        result = subprocess.call(args + ['--emit-deps-json', str(graph)])
        if result:
            return result
        # The compiler warns rather than fails if its graph cannot be written.
        # Fail closed here; an old object/dependency pair must remain untouched.
        data = json.loads(graph.read_text())
        if not isinstance(data.get('edges'), list):
            raise ValueError('missing compiler dependency edges')
        paths = {source}
        for edge in data['edges']:
            for key in ('from', 'to'):
                value = edge[key]
                make_path(value)
                if not Path(value).is_file():
                    raise ValueError('unavailable compiler dependency: ' + value)
                paths.add(value)
        dependencies = sorted(paths)
        dep.write_text(make_path(str(output)) + ': ' +
                       ' '.join(make_path(p) for p in dependencies) + '\n' +
                       ''.join(make_path(p) + ':\n' for p in dependencies))
        # The object is the commit point. Missing/invalid graph never installs it.
        os.replace(dep, output.with_suffix('.d'))
        os.replace(obj, output)
    return 0


def main():
    command = sys.argv[1:]
    if command[:1] == ['--']:
        command = command[1:]
    try:
        return compile_command(command)
    except (OSError, ValueError, KeyError, TypeError, IndexError) as error:
        print('fisiCs dependency publication failed: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
