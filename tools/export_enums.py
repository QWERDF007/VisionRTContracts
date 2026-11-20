"""Derive QML and Python enum exports from the public C++ definitions."""
import argparse
import keyword
import re
from pathlib import Path


def read_enums(headers):
    enums = []
    for header in headers:
        source = re.sub(r'//[^\n]*', '', Path(header).read_text(encoding='utf-8'))
        for name, body in re.findall(r'enum class (\w+)(?:\s*:\s*\w+(?:::\w+)*)?\s*\{([^}]+)\}', source):
            values, current = [], -1
            for member in body.split(','):
                member = member.strip()
                if not member:
                    continue
                parts = member.split('=')
                current = int(parts[1].strip(), 0) if len(parts) == 2 else current + 1
                values.append((parts[0].strip(), current))
            enums.append((name, values))
    return enums


def python_source(enums):
    lines = ['from enum import IntEnum', '']
    for name, values in enums:
        lines += [f'class {name}(IntEnum):'] + [f'    {key + chr(95) if keyword.iskeyword(key) else key} = {value}' for key, value in values] + ['']
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser()
    output = parser.add_mutually_exclusive_group(required=True)
    output.add_argument('--qml')
    output.add_argument('--python')
    parser.add_argument('headers', nargs='+')
    args = parser.parse_args()
    enums = read_enums(args.headers)
    if args.python:
        path, source = args.python, python_source(enums)
    else:
        lines = ['#pragma once', '#include <QObject>']
        lines += [f'#include "{Path(header).as_posix()}"' for header in args.headers]
        lines += ['namespace visionrt::qml {']
        for name, values in enums:
            lines += [f'class {name} {{', '    Q_GADGET', 'public:', '    enum Value {']
            lines += [f'        {key} = static_cast<int>(visionrt::{name}::{key}),' for key, _ in values]
            lines += ['    };', '    Q_ENUM(Value)', '};']
        path, source = args.qml, '\n'.join(lines + ['}', ''])
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text(source, encoding='utf-8')


if __name__ == '__main__':
    main()
