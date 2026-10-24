import sys, math, itertools
sys.path.insert(0, '.')
from sim import *
P = ['FJ','HI','HE','IM','DU','ST','MA','JA','RI','SU','SY','SS']
SH = {  # HumanLook sheen per rank
 'FJ':(3,4,4),'HI':(1,8,8),'HE':(2,8,8),'IM':(8,2,2),'DU':(2,4,4),'ST':(3,2,5),
 'MA':(1,3,3),'JA':(3,2,6),'RI':(2,8,8),'SU':(8,3,3),'SY':(7,2,6),'SS':(7,2,5)}
SLOTS = [('BRONZE',0),('BRASS',0),('IRON',0),('STEEL',1),('SILVER',1),('STEEL',2),('SILVER',2),('GLASS',2)]
# T[slot][people] = (name, h, C, v)
T = {
'BRONZE':[('SEA BRONZE',150,30,-1),('PEAT BRONZE',48,32,-1),('AMBER BRONZE',66,46,0),('RED BRONZE',38,50,0),
          ('SUN BRONZE',76,52,1),('RED COPPER',30,54,-1),('OLIVE BRONZE',104,36,-1),('BLACK BRONZE',62,24,-2),
          ('ROSE COPPER',22,42,0),('GOLD BRONZE',86,48,0),('MOSS BRONZE',128,38,0),('WHITE BRONZE',72,26,1)],
'BRASS':[('PALE BRASS',96,34,1),('OLD BRASS',80,38,-1),('BRASS',90,58,1),('RED BRASS',60,54,0),
         ('SUN BRASS',84,62,0),('DARK BRASS',76,42,-2),('GREEN BRASS',108,46,0),('TEMPLE BRASS',70,48,-1),
         ('LEMON BRASS',100,60,0),('ROYAL BRASS',78,56,1),('LEAF BRASS',114,40,1),('STAR BRASS',94,30,2)],
'IRON':[('RIME IRON',225,16,-1),('BOG IRON',75,12,-1),('BLUED IRON',234,26,0),('BLACK IRON',270,6,-2),
        ('DESERT IRON',50,16,0),('SKY IRON',268,26,0),('MOSS IRON',140,18,-1),('INK IRON',300,16,-1),
        ('SLATE IRON',200,14,0),('PLUM IRON',330,14,-2),('PALE IRON',160,12,0),('STAR IRON',255,12,1)],
'STEEL1':[('FROST STEEL',215,16,1),('HEATHER STEEL',310,20,0),('WHITE STEEL',240,6,1),('RED STEEL',18,30,0),
          ('DAMASK STEEL',60,12,0),('SKY STEEL',250,32,0),('MARSH STEEL',165,20,0),('JADE STEEL',175,30,0),
          ('RIVER STEEL',198,24,1),('BLACK STEEL',280,8,0),('LEAF STEEL',130,22,1),('VIOLET STEEL',290,32,1)],
'SILVER1':[('ICE SILVER',215,10,2),('MIST SILVER',150,8,2),('SILVER',250,4,2),('ROSE SILVER',345,12,2),
           ('SAND SILVER',80,14,2),('BLUE SILVER',255,16,2),('GREY SILVER',190,6,1),('MOON SILVER',280,12,2),
           ('PEARL SILVER',100,6,2),('SHADE SILVER',300,8,1),('MINT SILVER',160,16,2),('STAR SILVER',240,12,2)],
'STEEL2':[('AURORA STEEL',165,32,0),('THISTLE STEEL',300,36,0),('COBALT STEEL',248,38,0),('PURPLE STEEL',305,38,-1),
          ('TURQUOISE STEEL',195,36,0),('TEMPER STEEL',250,32,0),('BOG STEEL',120,28,-1),('SPIRIT STEEL',160,32,-1),
          ('SUNSET STEEL',40,32,0),('NIGHT STEEL',280,20,-1),('THORN STEEL',140,32,0),('COMET STEEL',270,42,0)],
'SILVER2':[('AURORA SILVER',180,18,2),('HEATH SILVER',320,16,2),('SUN SILVER',95,14,2),('EAGLE SILVER',260,14,2),
           ('OASIS SILVER',190,16,2),('STORM SILVER',240,18,2),('WISP SILVER',140,16,1),('SPIRIT SILVER',170,18,1),
           ('MOON SILVER',280,16,2),('ECLIPSE SILVER',300,12,1),('DEW SILVER',120,16,1),('STAR SILVER',250,22,2)],
'GLASS2':[('ICE GLASS',205,34,0),('PEAT GLASS',60,30,-1),('SAPPHIRE GLASS',245,40,0),('GARNET GLASS',20,34,-1),
          ('AMBER GLASS',75,40,0),('SKY GLASS',230,30,0),('BOG GLASS',110,30,-1),('JADE GLASS',160,34,0),
          ('RIVER GLASS',195,30,0),('NIGHT GLASS',300,26,-1),('VERDANT GLASS',150,38,0),('STAR GLASS',290,40,0)],   # (forge C15f: spread)
}
def key(slot, rank):
    return slot + ('' if slot in ('BRONZE','BRASS','IRON') else str(rank))
meet = set()
for ln in open('meet.txt'):
    a,b = map(int, ln.split()); meet.add((a,b))
def mid(sk, p):
    n,h,C,v = T[sk][p]; rank = int(sk[-1]) if sk[-1].isdigit() else 0
    return ramp(h,C,v,SH[P[p]][rank])
if __name__ == "__main__":
    fails = 0
    for sk in T:
        rank = int(sk[-1]) if sk[-1].isdigit() else 0
        for p in range(12):
            n,h,C,v = T[sk][p]
            ok, l = metal_ok(mid(sk,p))
            if not ok: print('METAL', sk, P[p], n, l); fails += 1
    # within
    R0, R1, R2 = ['BRONZE','BRASS','IRON'], ['STEEL1','SILVER1'], ['STEEL2','SILVER2']
    mn_in = 99
    for p in range(12):
        r2 = R2 + (['GLASS2'] if P[p] in ('SY','SS') else [])
        for a,b,c in itertools.product(R0,R1,r2):
            ms = [mid(s,p)[2] for s in (a,b,c)]
            for (i,j) in ((0,1),(0,2),(1,2)):
                d = dE(ms[i],ms[j]); mn_in = min(mn_in,d)
                if d < 20: print('IN', P[p], (a,b,c)[i], (a,b,c)[j], round(d,1)); fails += 1
    mn_nb = 99; mn_x = 99
    for (a,b) in meet:
        for sk in T:
            if sk == 'GLASS2': continue
            d = dE(mid(sk,a)[2], mid(sk,b)[2]); mn_nb = min(mn_nb,d)
            if d < 12: print('NB', sk, P[a], P[b], T[sk][a][0], T[sk][b][0], round(d,1)); fails += 1
        for grp in (R0,R1,R2):
            for s1 in grp:
                for s2 in grp:
                    if s1 == s2: continue
                    d = dE(mid(s1,a)[2], mid(s2,b)[2]); mn_x = min(mn_x,d)
                    if d < 8: print('X', P[a], s1, P[b], s2, round(d,1)); fails += 1
    print('fails', fails, 'min in %.1f nb %.1f x %.1f' % (mn_in, mn_nb, mn_x))
    if '--img' in sys.argv:
        from PIL import Image, ImageDraw
        cw, chh = 60, 14
        im = Image.new('RGB', (12*cw+40, len(T)*chh+10), (40,40,40))
        for si, sk in enumerate(T):
            for p in range(12):
                r = mid(sk,p)
                for k in range(5):
                    for x in range(10):
                        for y in range(10): im.putpixel((40+p*cw+k*10+x, 5+si*chh+y), r[k])
        im = im.resize((im.width*3, im.height*3), Image.NEAREST); im.save('swatch.png')
    