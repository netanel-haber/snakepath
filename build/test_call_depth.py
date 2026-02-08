#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path

FUNC_RE = re.compile(
    r"^\s*(?:static\s+inline\s+|static\s+)?(?:[A-Za-z_][\w\s\*]*\s+)?(sp_[A-Za-z0-9_]+)\s*\("
)
CALL_RE = re.compile(r"\b(sp_[A-Za-z0-9_]+)\s*\(")


def collect_definitions(lines: list[str]) -> dict[str, tuple[int, int]]:
    definitions: dict[str, tuple[int, int]] = {}
    i = 0
    while i < len(lines):
        line = lines[i]
        if line.lstrip().startswith("#"):
            i += 1
            continue

        match = FUNC_RE.match(line)
        if not match:
            i += 1
            continue

        name = match.group(1)
        signature = line
        j = i
        while "{" not in signature and j + 1 < len(lines):
            j += 1
            signature += "\n" + lines[j]
        if "{" not in signature:
            i = j + 1
            continue

        if ";" in signature.split("{", 1)[0]:
            i = j + 1
            continue

        brace_depth = signature.count("{") - signature.count("}")
        k = j
        while brace_depth > 0 and k + 1 < len(lines):
            k += 1
            brace_depth += lines[k].count("{") - lines[k].count("}")

        definitions[name] = (i, k)
        i = k + 1
    return definitions


def longest_path_from(start: str, graph: dict[str, set[str]]) -> list[str]:
    best = [start]

    def dfs(node: str, path: list[str], seen: set[str]) -> None:
        nonlocal best
        if len(path) > len(best):
            best = path.copy()
        for next_node in graph.get(node, set()):
            if next_node in seen:
                continue
            seen.add(next_node)
            path.append(next_node)
            dfs(next_node, path, seen)
            path.pop()
            seen.remove(next_node)

    dfs(start, [start], {start})
    return best


def main() -> int:
    header_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("snakepath.h")
    max_depth = int(sys.argv[2]) if len(sys.argv) > 2 else 3

    lines = header_path.read_text(encoding="utf-8").splitlines()
    definitions = collect_definitions(lines)
    names = set(definitions)

    public = {
        name
        for name in names
        if name.startswith("sp_")
        and not name.startswith("sp_priv_")
        and not name.startswith("sp_fluent_")
    }

    graph: dict[str, set[str]] = {name: set() for name in public}
    for name in public:
        start, end = definitions[name]
        body = "\n".join(lines[start : end + 1])
        for called in CALL_RE.findall(body):
            if called in public and called != name:
                graph[name].add(called)

    offenders: list[tuple[str, list[str]]] = []
    max_seen = 0
    for root in sorted(public):
        path = longest_path_from(root, graph)
        depth = len(path)
        if depth > max_seen:
            max_seen = depth
        if depth > max_depth:
            offenders.append((root, path))

    print(
        f"call-depth check: public_functions={len(public)} max_depth={max_seen} limit={max_depth}"
    )
    if not offenders:
        print("call-depth check: OK")
        return 0

    print("call-depth check: FAILED")
    for root, path in offenders:
        print(f"  {root}: {' -> '.join(path)}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
