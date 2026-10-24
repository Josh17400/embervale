import math, itertools, sys
# replica of doll_alloy.cpp famRamp (the i3 model) for tuning the family table
def lin(c):
    c/=255.0; return c/12.92 if c<=0.04045 else ((c+0.055)/1.055)**2.4
def unlin(c):
    v = 12.92*c if c<=0.0031308 else 1.055*(max(c,0.0)**(1/2.4))-0.055
    return min(255.0,max(0.0,v*255.0))
def toLab(rgb):
    r,g,b=[lin(x) for x in rgb]
    X=(0.4124*r+0.3576*g+0.1805*b)/0.95047; Y=0.2126*r+0.7152*g+0.0722*b; Z=(0.0193*r+0.1192*g+0.9505*b)/1.08883
    f=lambda t: t**(1/3) if t>0.008856 else 7.787*t+16/116
    fx,fy,fz=f(X),f(Y),f(Z); return (116*fy-16,500*(fx-fy),200*(fy-fz))
def fromLab(l):
    L,a,b=l; fy=(L+16)/116; fx=fy+a/500; fz=fy-b/200
    fi=lambda t: t**3 if t**3>0.008856 else (t-16/116)/7.787
    X=fi(fx)*0.95047; Y=fi(fy); Z=fi(fz)*1.08883
    r=3.2406*X-1.5372*Y-0.4986*Z; g=-0.9689*X+1.8758*Y+0.0415*Z; bb=0.0557*X-0.2040*Y+1.0570*Z
    return tuple(int(round(unlin(v))) for v in (r,g,bb))
def luma(c): return c[0]*0.299+c[1]*0.587+c[2]*0.114
def dE(x,y):
    a,b=toLab(x),toLab(y); return math.sqrt(sum((p-q)**2 for p,q in zip(a,b)))
def hueTurn(h,t,m):
    d=math.fmod(t-h+540.0,360.0)-180.0; return h+max(-m,min(m,d))
def lch(L,C,h): r=math.radians(h); return (L,C*math.cos(r),C*math.sin(r))
SKIN=(0xE9,0xA0,0x77)
KLV={-2:[5,28,38,66,94],-1:[7,30,44,72,96],0:[9,33,52,80,98],1:[13,40,62,87,99],2:[18,48,71,92,100]}
KCF=[0.7,1.1,1.2,0.55,0.12]
def ramp(h,C,v,sheen):
    L=list(KLV[v]); Cs=[C*k for k in KCF]; H=[h]*5
    H[0]=hueTurn(h,290,22); H[1]=hueTurn(h,290,10); H[3]=hueTurn(h,90,8); H[4]=hueTurn(h,90,14)
    if h>=330 or h<40: H[3]=hueTurn(h,90,14); H[4]=hueTurn(h,90,30); Cs[3]*=0.8
    if sheen==1:   # matte: the top softer, no loss of the glint
        L[4]=L[3]+(L[4]-L[3])*0.85; Cs[3]*=1.05; Cs[4]=Cs[3]*0.5; L[0]+=2
    elif sheen==2:
        L[4]=99; Cs[4]=C*0.1; L[3]+=3; L[2]-=3
    elif sheen in (3,6):
        for i in range(4): L[i]= max(31.0,L[i]*0.84) if i==1 else L[i]*(0.94 if i==3 else 0.82)
        for i in range(1,4): Cs[i]*=0.85
        H[4]=hueTurn(h,250,60); Cs[4]=10; L[4]=max(L[4]-2,90.0)
    elif sheen==5:
        H[0]=h-50; H[1]=h-24; H[3]=h+30; H[4]=h+56
        for i in range(4): Cs[i]*=1.1
        Cs[4]=C*0.42
    elif sheen==7:
        for i in range(1,5): L[i]=min(99.0,L[i]+5)
        for i in range(5): Cs[i]*=0.75
    elif sheen==8:
        if abs((h-80+180)%360-180) <= 60: H[3]=hueTurn(h,80,12); H[4]=hueTurn(h,80,22); Cs[4]=C*0.44
        else: Cs[4]=C*0.3
        Cs[3]*=1.1; L[4]=97
    out=[fromLab(lch(L[i],Cs[i],H[i])) for i in range(5)]
    sk=toLab(SKIN)
    for i in (2,3):
        it=0
        while it<24 and math.dist(toLab(out[i]),sk)<22.0:
            Cs[i]*=1.05; H[i]+= -2.0 if (H[i]<55 or H[i]>235) else 2.0
            if i==2: L[i]-=0.8
            out[i]=fromLab(lch(L[i],Cs[i],H[i])); it+=1
    it=0
    while it<30 and luma(out[1])<82.0 and L[1]<L[2]-6:
        L[1]+=1; out[1]=fromLab(lch(L[1],Cs[1],H[1])); it+=1
    return out
def metal_ok(r):
    l=[luma(c) for c in r]
    return l[4]>=196 and l[4]-l[0]>=165 and l[0]<=56 and l[3]-l[1]>=60, [round(x) for x in l]
