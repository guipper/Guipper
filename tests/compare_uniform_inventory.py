"""Compare inventories emitted by GUIPPER_PERSISTENCE_TEST=uniform_inventory."""
import copy
import json
import sys
from pathlib import Path


def canonical(row):
    result = copy.deepcopy(row)
    for parameter in result["parameters"]:
        parameter["name"] = parameter["name"].strip()
    result["inputs"] = [name.strip() for name in result["inputs"]]
    return result


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 tests/compare_uniform_inventory.py BEFORE.json AFTER.json")
        return 2
    before, after = (json.loads(Path(path).read_text()) for path in sys.argv[1:])
    exact, whitespace, changed = [], [], []
    for name in sorted(before.keys() | after.keys()):
        if name not in before or name not in after:
            changed.append(name)
        elif before[name] == after[name]:
            exact.append(name)
        elif canonical(before[name]) == canonical(after[name]):
            whitespace.append(name)
        else:
            changed.append(name)
    print(f"{len(exact)} exact; {len(whitespace)} whitespace-only; {len(changed)} other changes")
    for name in whitespace:
        print(f"  legacy name whitespace: {name}")
    for name in changed:
        print(f"  REVIEW: {name}")
    return 1 if changed else 0


if __name__ == "__main__":
    sys.exit(main())
