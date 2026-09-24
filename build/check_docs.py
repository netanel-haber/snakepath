#!/usr/bin/env python3
"""Fail if README.md, docs/index.html and api_demo.c drift apart.

README.md's code blocks must match docs/index.html's <pre> blocks (ignoring the syntax
highlighting markup), and README.md must embed api_demo.c verbatim.
"""
import html
import re
import sys


def read(path):
    with open(path, encoding='utf-8') as f:
        return f.read()


root = sys.argv[1] if len(sys.argv) > 1 else '..'
demo = read(f'{root}/api_demo.c')
readme_blocks = re.findall(r'```\w*\n(.*?)```', read(f'{root}/README.md'), re.S)
page_blocks = [html.unescape(re.sub(r'<[^>]+>', '', block))
               for block in re.findall(r'<pre[^>]*>(.*?)</pre>', read(f'{root}/docs/index.html'), re.S)]

problems = []
if demo not in readme_blocks:
    problems.append('README.md does not embed api_demo.c verbatim')
if [b.rstrip('\n') for b in readme_blocks] != [b.rstrip('\n') for b in page_blocks]:
    problems.append('docs/index.html code blocks differ from README.md')
for problem in problems:
    print(f'docs check: {problem}')
print('docs check: FAILED' if problems else 'docs check: OK')
sys.exit(1 if problems else 0)
