import sys, math, random, itertools, pickle, os
sys.path.insert(0, '.')
import sim
from table import P, SH, T, meet
HS = list(range(0, 360, 10))
CS = [4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62]
VS = [-2,-1,0,1,2]
CACHE = 'grid.pkl'
if os.path.exists(CACHE):
    G = pickle.load(open(CACHE,'rb'))
else:
    G = {}
    for h in HS:
        for C in CS:
            for v in VS:
                ok = True; mids = {}
                for sh in range(9):
                    r = sim.ramp(h,C,v,sh)
                    ok &= sim.metal_ok(r)[0]
                    ok &= sim.metal_read(r)   # (forge C17)
                    sk = sim.toLab(sim.SKIN)
                    ok &= min(math.dist(sim.toLab(r[2]),sk), math.dist(sim.toLab(r[3]),sk)) >= 20.5
                    mids[sh] = sim.toLab(r[2])
                G[(h,C,v)] = (ok, mids)
    pickle.dump(G, open(CACHE,'wb'))
def hd(a,b): return abs((a-b+180)%360-180)
# allowed region per slot: (hue ok fn, Cmin, Cmax, vset)
def warmband(h): return 15 <= h <= 115
REG = {
 'BRONZE': (lambda h,C: (18<=h<=110) or (120<=h<=170 and C<=36), 22, 56, (-2,-1,0,1)),
 'BRASS':  (lambda h,C: 55<=h<=118 or C<=14, 12, 62, (-2,-1,0,1,2)),   # (forge C15f: a pale brass may go near-neutral)
 'IRON':   (lambda h,C: C<=12 or not (40<=h<=110), 4, 26, (-2,-1,0)),
 'STEEL1': (lambda h,C: C<=12 or not (15<=h<=120), 4, 34, (-1,0,1)),
 'SILVER1':(lambda h,C: C<=12 or not (h<=130 or h>=345), 4, 18, (1,2)),
 'STEEL2': (lambda h,C: C<=14 or not (15<=h<=120), 12, 46, (-1,0,1)),
 'SILVER2':(lambda h,C: C<=12 or not (h<=130 or h>=345), 6, 24, (1,2)),
 'GLASS2': (lambda h,C: True, 26, 46, (-1,0)),
}
SLOTRANK = {'BRONZE':0,'BRASS':0,'IRON':0,'STEEL1':1,'SILVER1':1,'STEEL2':2,'SILVER2':2,'GLASS2':2}
slots = [s for s in T]
cands = {}
for s in slots:
    f, cmin, cmax, vs = REG[s]
    cands[s] = [k for k in G if G[k][0] and cmin <= k[1] <= cmax and k[2] in vs and f(k[0],k[1])]
    print(s, len(cands[s]))
PIN = {('IRON',2): lambda k: k[2]==0 and k[1]>=20 and 240<=k[0]<=260, ('STEEL2',8): lambda k: k[1]>=28, ('STEEL2',9): lambda k: k[1]>=24}
def seedcost(s, p, k):
    n,h,C,v = T[s][p]
    return (hd(k[0],h)/30.0)**2*(min(C,k[1])/20.0) + ((k[1]-C)/12.0)**2 + (k[2]-v)**2*0.5
def nearest(s,p):
    return min(cands[s], key=lambda k: seedcost(s,p,k))
import colorsys
def pinkish(k, sh):
    r = sim.ramp(k[0],k[1],k[2],sh)
    for c in (r[2], r[3]):
        hh,ss,vv = colorsys.rgb_to_hsv(*[x/255 for x in c])
        hh *= 360
        if (hh >= 290 or hh <= 20) and 0.14 <= ss and vv > 0.5: return True
        if (hh >= 290 or hh <= 20) and ss > 0.3 and vv > 0.42: return True
    return False
def brownish(k, sh):
    r = sim.ramp(k[0],k[1],k[2],sh)
    for c in (r[1], r[2], r[3]):
        hh,ss,vv = colorsys.rgb_to_hsv(*[x/255 for x in c]); hh *= 360
        if (hh >= 280 or hh <= 75) and ss >= 0.06: return True
    return False
ckey = {(s,p): [k for k in cands[s] if PIN.get((s,p), lambda k: True)(k) and not pinkish(k, SH[P[p]][SLOTRANK[s]]) and (s in ('BRONZE','BRASS') or not brownish(k, SH[P[p]][SLOTRANK[s]]))] for s in slots for p in range(12)}
state = {(s,p): min(ckey[(s,p)], key=lambda k: seedcost(s,p,k)) for s in slots for p in range(12)}
def mid(s,p,k=None):
    k = k or state[(s,p)]
    return G[k][1][SH[P[p]][SLOTRANK[s]]]
R0, R1 = ['BRONZE','BRASS','IRON'], ['STEEL1','SILVER1']
def r2of(p): return ['STEEL2','SILVER2'] + (['GLASS2'] if P[p] in ('SY','SS') else [])
pairsIn = []  # (s1,p,s2,p,thr)
for p in range(12):
    groups = [R0,R1,r2of(p)]
    for gi,gj in ((0,1),(0,2),(1,2)):
        for s1 in groups[gi]:
            for s2 in groups[gj]: pairsIn.append((s1,p,s2,p,23.5))
pairsNb = []
for (a,b) in meet:
    for s in slots:
        if s != 'GLASS2': pairsNb.append((s,a,s,b,13.5))
    for grp in (R0,R1,['STEEL2','SILVER2']):
        for s1 in grp:
            for s2 in grp:
                if s1 != s2: pairsNb.append((s1,a,s2,b,9.5))
ALL = pairsIn + pairsNb
inv = {}
for i,(s1,p1,s2,p2,t) in enumerate(ALL):
    inv.setdefault((s1,p1),[]).append(i); inv.setdefault((s2,p2),[]).append(i)
def pc(i):
    s1,p1,s2,p2,t = ALL[i]
    d = math.dist(mid(s1,p1), mid(s2,p2))
    return max(0.0, t-d)**2
# (forge C15f) the spread over all 96 families' plain mid steps: no dE-10 ball holds more than 3 (pairs under 11 cost), and
# each slot's 12 families span 3 or more hue sextants
KEYS = [(s,p) for s in slots for p in range(12)]
def mid0(key): return G[state[key]][1][0]
def spreadPair(k1, k2):
    d = math.dist(mid0(k1), mid0(k2))
    return 0.6*max(0.0, 11.5-d)**2
RGB0 = {}
def rgb0(k):
    if k not in RGB0: RGB0[k] = sim.ramp(*k, 0)[2]
    return RGB0[k]
def sextants(s):
    out = set()
    for p in range(12):
        r = rgb0(state[(s,p)])
        hh, ss, vv = colorsys.rgb_to_hsv(*[x/255 for x in r])
        out.add(6 if max(r)-min(r) < 21 else int(hh*6) % 6)
    return len(out)
def spreadLocal(key):
    return sum(spreadPair(key, o) for o in KEYS if o != key) + 60.0*max(0, 3-sextants(key[0]))
def local(key):
    return sum(pc(i) for i in inv.get(key,[])) + 0.6*seedcost(key[0],key[1],state[key]) + spreadLocal(key)
def total():
    sp = sum(spreadPair(KEYS[i], KEYS[j]) for i in range(len(KEYS)) for j in range(i+1, len(KEYS))) + sum(60.0*max(0, 3-sextants(s)) for s in slots)
    return sum(pc(i) for i in range(len(ALL))) + 0.6*sum(seedcost(s,p,state[(s,p)]) for (s,p) in state) + sp
if __name__ == '__main__':
  random.seed(int(sys.argv[1]) if len(sys.argv)>1 else 1)
  global_dummy=0
  cur = total(); print('start', cur)
  Tt = 20.0
  keys = list(state)
  for it in range(int(sys.argv[2]) if len(sys.argv)>2 else 120000):
      key = random.choice(keys)
      old = state[key]
      before = local(key)
      state[key] = random.choice(ckey[key])
      after = local(key)
      dlt = after - before
      if dlt <= 0 or random.random() < math.exp(-dlt/Tt): cur += dlt
      else: state[key] = old
      Tt *= 0.99995
  print('end', total())
  viol = [(ALL[i], round(math.dist(mid(ALL[i][0],ALL[i][1]),mid(ALL[i][2],ALL[i][3])),1)) for i in range(len(ALL)) if pc(i)>0]
  print('violations', len(viol))
  balls = max(sum(1 for o in KEYS if math.dist(mid0(k), mid0(o)) <= 10.0) for k in KEYS)
  print('C15f densest ball', balls, 'sextants', [sextants(s) for s in slots])
  for v in viol[:40]: print(v)
  pickle.dump(state, open('state.pkl','wb'))
  for s in slots:
      print(s, [(P[p],)+state[(s,p)] for p in range(12)])
  