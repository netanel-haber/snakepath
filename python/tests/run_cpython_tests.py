#!/usr/bin/env python3
"""
Run CPython's pathlib PUBLIC API test suite against snakepath.

Downloads test_join_posix.py and test_join_windows.py from CPython
and runs only the PurePosixPath/PureWindowsPath tests (skips internal API tests).
"""

import sys
import os
import unittest
import urllib.request

# Add our module to path FIRST
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import snakepath

# Patch pathlib BEFORE test discovery
class PatchedPathlib:
    PurePath = snakepath.PurePath
    PurePosixPath = snakepath.PurePosixPath
    PureWindowsPath = snakepath.PureWindowsPath
    Path = snakepath.Path
    PosixPath = snakepath.PosixPath
    WindowsPath = snakepath.WindowsPath

class PatchedPathlibOs:
    @staticmethod
    def vfspath(p):
        return str(p)

sys.modules['pathlib'] = PatchedPathlib()
sys.modules['pathlib._os'] = PatchedPathlibOs()

CPYTHON_RAW = "https://raw.githubusercontent.com/python/cpython/main/Lib/test/test_pathlib"

FILES = [
    "test_join_posix.py",
    "test_join_windows.py",
]


def download_file(url, dest):
    """Download a file."""
    print(f"  {os.path.basename(dest)}")
    with urllib.request.urlopen(url) as r:
        content = r.read()
    with open(dest, 'wb') as f:
        f.write(content)


def setup_test_dir():
    """Setup test directory with downloaded tests and support stubs."""
    test_dir = os.path.join(os.path.dirname(__file__), "cpython_tests")
    os.makedirs(test_dir, exist_ok=True)

    # Create __init__.py
    with open(os.path.join(test_dir, "__init__.py"), "w") as f:
        f.write("")

    # Create support package with stubs
    support_dir = os.path.join(test_dir, "support")
    os.makedirs(support_dir, exist_ok=True)

    with open(os.path.join(support_dir, "__init__.py"), "w") as f:
        f.write("is_pypi = False\n")

    with open(os.path.join(support_dir, "lexical_path.py"), "w") as f:
        f.write("""# Stub - these tests are skipped
class LexicalPath: pass
class LexicalPosixPath: pass
class LexicalWindowsPath: pass
""")

    # Download test files if needed
    for filename in FILES:
        dest = os.path.join(test_dir, filename)
        if not os.path.exists(dest):
            print("Downloading CPython pathlib tests...")
            url = f"{CPYTHON_RAW}/{filename}"
            download_file(url, dest)

    return test_dir


def run_tests():
    """Run CPython tests against snakepath."""
    test_dir = setup_test_dir()

    # Add to path for imports
    sys.path.insert(0, os.path.dirname(test_dir))

    print("\n" + "=" * 70)
    print("Running CPython pathlib PUBLIC API tests against snakepath")
    print("(Skipping LexicalPath tests - those use internal APIs)")
    print("=" * 70 + "\n")

    # Discover and filter tests
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()

    for filename in FILES:
        module_name = f"cpython_tests.{filename[:-3]}"
        try:
            __import__(module_name)
            module = sys.modules[module_name]

            for name in dir(module):
                obj = getattr(module, name)
                if isinstance(obj, type) and issubclass(obj, unittest.TestCase):
                    if 'Lexical' in name:
                        print(f"  SKIP: {name} (internal API)")
                        continue
                    print(f"  LOAD: {name}")
                    suite.addTests(loader.loadTestsFromTestCase(obj))
        except Exception as e:
            print(f"  ERROR loading {filename}: {e}")

    print()

    # Run
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    # Summary
    print("\n" + "=" * 70)
    print(f"Ran {result.testsRun} CPython tests against snakepath")
    if result.wasSuccessful():
        print("SUCCESS")
    else:
        print(f"FAILURES: {len(result.failures)}, ERRORS: {len(result.errors)}")
    print("=" * 70)

    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(run_tests())
