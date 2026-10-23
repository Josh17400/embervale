#!/usr/bin/env python3
"""Embed tools/doll/ladder_looks.txt as the gates' matrix (tools/tests/test_doll_arms.cpp kSliceLooks). LADDER-DOLL.
Run after editing the looks file: python tools/doll/embed_looks.py [--check]"""
import re, sys, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[2]
src = (ROOT / 'tools/doll/ladder_looks.txt').read_text(encoding='utf-8').splitlines()
lines = [l for l in src if l.strip() and not l.startswith('#')]
BS = chr(92)
def cstr(l):
    parts = re.sub(r'\s+', ' ', l.strip()).split(' ')
    full = f'{parts[0]:<8} {parts[1]:<6} {parts[2]}  {parts[3] if len(parts) > 3 else ""}' + BS + 'n'
    out = []
    while len(full) > 140:
        cut = full.rfind(',', 0, 140) + 1
        out.append(full[:cut]); full = full[cut:]
    out.append(full)
    return '\n'.join('    "' + o + '"' for o in out)
body = ("const char* kSliceLooks =   // (P2c ladder) the gates' matrix: tools/doll/ladder_looks.txt (tools/doll/embed_looks.py writes it)\n"
        + '\n'.join(cstr(l) for l in lines) + ';\n')
p = ROOT / 'tools/tests/test_doll_arms.cpp'
s = p.read_text(encoding='utf-8')
i = s.index('const char* kSliceLooks ='); j = s.index(';\n', i) + 2
if '--check' in sys.argv:
    ok = s[i:j] == body
    print('embed_looks: ' + ('in step' if ok else 'STALE: run python tools/doll/embed_looks.py'))
    sys.exit(0 if ok else 1)
p.write_text(s[:i] + body + s[j:], encoding='utf-8', newline='\n')
print('embed_looks: %d looks embedded' % len(lines))
