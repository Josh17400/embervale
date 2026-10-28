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
    if not ok: sys.exit(1)
else:
    p.write_text(s[:i] + body + s[j:], encoding='utf-8', newline='\n')
    print('embed_looks: %d looks embedded' % len(lines))

# (P2c envelope, FORGE-DOLL) tools/doll/envelope_looks.txt as tools/tests/test_doll_envelope.cpp kEnvelopeLooks
esrc = (ROOT / 'tools/doll/envelope_looks.txt').read_text(encoding='utf-8').splitlines()
elines = [l for l in esrc if l.strip() and not l.startswith('#')]
ebody = ("const char* kEnvelopeLooks =   // (P2c envelope) tools/doll/envelope_looks.txt (tools/doll/embed_looks.py writes it)\n"
         + '\n'.join(cstr(l) for l in elines) + ';\n')
ep = ROOT / 'tools/tests/test_doll_envelope.cpp'
es = ep.read_text(encoding='utf-8')
ei = es.index('const char* kEnvelopeLooks ='); ej = es.index(';\n', ei) + 2
if '--check' in sys.argv:
    eok = es[ei:ej] == ebody
    print('embed_looks (envelope): ' + ('in step' if eok else 'STALE: run python tools/doll/embed_looks.py'))
    if not eok: sys.exit(1)
else:
    ep.write_text(es[:ei] + ebody + es[ej:], encoding='utf-8', newline='\n')
    print('embed_looks: %d envelope looks embedded' % len(elines))

# (P2c forge, FORGE-DOLL) tools/doll/forge_looks.txt as tools/tests/test_doll_forge.cpp kForgeLooks (the gates' matrix adds
# its tags that the ladder file lacks: test_doll_arms.cpp dollSliceLooksText)
fsrc = (ROOT / 'tools/doll/forge_looks.txt').read_text(encoding='utf-8').splitlines()
flines = [l for l in fsrc if l.strip() and not l.startswith('#')]
fbody = ("const char* kForgeLooks =   // (P2c forge) tools/doll/forge_looks.txt (tools/doll/embed_looks.py writes it)\n"
         + '\n'.join(cstr(l) for l in flines) + ';\n')
fp = ROOT / 'tools/tests/test_doll_forge.cpp'
fs = fp.read_text(encoding='utf-8')
fi = fs.index('const char* kForgeLooks ='); fj = fs.index(';\n', fi) + 2
if '--check' in sys.argv:
    ok = fs[fi:fj] == fbody
    print('embed_looks (forge): ' + ('in step' if ok else 'STALE: run python tools/doll/embed_looks.py'))
    sys.exit(0 if ok else 1)
fp.write_text(fs[:fi] + fbody + fs[fj:], encoding='utf-8', newline='\n')
print('embed_looks: %d forge looks embedded' % len(flines))
