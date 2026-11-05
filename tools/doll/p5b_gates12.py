#!/usr/bin/env python3
"""tools/doll/p5b_gates12.py (P5b production integration): the slice gates S1 (silhouette masks: figure, no shield, head) and R3a
(nearest-neighbour greyscale read) over ALL twelve peoples at T1-T4, each unordered pair once (STYLE_P5B 9.3 thresholds).
  fingerprint_gallery <dir> --slice --peoples <all twelve> --tiers 1,2,3,4 --seeds 1..3 ;  python tools/doll/p5b_gates12.py <dir>"""
# the slice's S1 and R3a (and N2) over ALL twelve peoples at T1-T4 (each unordered pair once); thresholds as STYLE_P5B 9.3 / p5b_pack_B.py
import os,sys,itertools
import numpy as np
from PIL import Image
D=sys.argv[1]
ALL=['fjordfolk','highland','heartland','imperial','dune','steppe','marsh','jade','river','suntemple','sylvan','starspire']
T=[1,2,3,4]; SEEDS=[1,2,3]
fp=lambda p,t,b,f,suf,s: os.path.join(D,'slice_%s_T%d_%s_s%d_%s%s.png'%(p,t,b,s,f,suf))
C={}
def arr(p,t,b,f,suf,s):
  k=(p,t,b,f,suf,s)
  if k not in C: C[k]=np.asarray(Image.open(fp(*k)).convert('RGB')).astype(np.int32)
  return C[k]
mask=lambda a:(a.sum(2)!=0)
L=[]; miss=[]; n=0; tight={}
for t in T:
  for p,q in itertools.combinations(ALL,2):
    for b in 'mw':
      for s in SEEDS:
        good=True; heads=[]; worst=9e9; parts=[]
        for f in 'DR':
          A,B=mask(arr(p,t,b,f,'',s)),mask(arr(q,t,b,f,'',s))
          An,Bn=mask(arr(p,t,b,f,'_ns',s)),mask(arr(q,t,b,f,'_ns',s))
          Ah,Bh=mask(arr(p,t,b,f,'_head',s)),mask(arr(q,t,b,f,'_head',s))
          x1=int((A^B).sum()); n1=max(12,min(A.sum(),B.sum())/25)
          x2=int((An^Bn).sum()); n2=max(8,min(An.sum(),Bn.sum())/40)
          x3=int((Ah^Bh).sum()); n3=max(6,min(Ah.sum(),Bh.sum())/5)
          heads.append((x3,n3)); good&= x1>=n1 and x2>=n2; worst=min(worst,x1/n1,x2/n2)
          parts.append('%s fig %d/%.0f ns %d/%.0f head %d/%.0f'%(f,x1,n1,x2,n2,x3,n3))
        hd=(heads[0][0]>=heads[0][1] and heads[1][0]>=3) or (heads[1][0]>=heads[1][1] and heads[0][0]>=3)
        good&=hd; worst=min(worst,max(heads[0][0]/heads[0][1],heads[1][0]/heads[1][1]))
        n+=1
        line='T%d %s s%d %-9s/%-9s %s'%(t,'man  ' if b=='m' else 'woman',s,p,q,'; '.join(parts))
        if not good: miss.append(line)
        if t not in tight or worst<tight[t][0]: tight[t]=(worst,line)
print('S1 silhouette over all 12 (66 pairs x T1-T4 x man/woman x seeds 1-3, facings D+R): %d pair-lines, %d miss -> %s'%(n,len(miss),'PASS' if not miss else 'FAIL'))
for t in T: print('  tightest T%d (x%.2f): %s'%(t,tight[t][0],tight[t][1]))
for m in miss: print('  MISS',m)
# R3a
bad=0; lines=0; worst=[]
for t in T:
  for b in 'mw':
    for f in 'DR':
      for var in ['','_walk','_ns']:
        keys=[(p,s) for p in ALL for s in SEEDS]
        G=np.stack([np.where(arr(p,t,b,f,var,s).sum(2)==0,-60.0,(arr(p,t,b,f,var,s)*np.array([.299,.587,.114])).sum(2)).ravel() for p,s in keys])
        Dm=np.abs(G[:,None,:]-G[None,:,:]).sum(2)
        for i,k in enumerate(keys):
          own=min(Dm[i,j] for j,k2 in enumerate(keys) if k2!=k and k2[0]==k[0])
          oth,oj=min((Dm[i,j],j) for j,k2 in enumerate(keys) if k2[0]!=k[0])
          lines+=1; bad+= oth<=own
          worst.append((oth/max(own,1),t,b,f,var or '_idle',k,keys[oj][0]))
worst.sort()
print('R3a nearest-neighbour read over all 12: %d of %d figures nearest another people -> %s'%(bad,lines,'PASS' if bad==0 else 'FAIL'))
for w in worst[:8]: print('  T%d %s %s %-6s %s s%d nearest other %s other/own %.2f%s'%(w[1],w[2],w[3],w[4],w[5][0],w[5][1],w[6],w[0],'  MISS' if w[0]<=1 else ''))
