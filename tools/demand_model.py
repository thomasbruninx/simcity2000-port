#!/usr/bin/env python3
"""Independent Python model of the demand valves (first half of SIMCITY.EXE 0x467AE0, docs/simulation/monthly-phases.md).
Python floats are IEEE doubles, matching the x87 53-bit precision of the original. Prints the vectors used by tests/test_economy.cpp."""
import math
K0, K1, K2, K3, K4 = 1.0, 1.0/150000.0, 0.01, 15.0, 600.0
ECON = [0.0, 1.2, 1.1, 0.95]
TAX = [200,160,120,100,75,50,25,0,-25,-50,-100,-150,-200,-250,-300,-350,-400,-450,-500,-550,-600,-650,-700,0]
def s16(v): v &= 0xffff; return v-0x10000 if v&0x8000 else v
def tdiv(a,b): return int(a/b) if (a<0)==(b<0) or a%b==0 and False else -((-a)//b) if (a<0)!=(b<0) and True else a//b
def cdiv(a,b):
    q=abs(a)//abs(b); return q if (a<0)==(b<0) else -q
def demand(zp, pop10_old, ratio_mem, c9200, econ, census, extra, tax, ordin, dem_old, c9408=0, cb4f0=0):
    zp=list(zp); total=sum(zp[1:7]); zp[0]=total; new10=total*10
    cat=[zp[1]+zp[2], zp[3]+zp[4], zp[5]+zp[6]]
    comind=0.0
    for i in (1,2): comind=(float(zp[1+2*i])+float(zp[2+2*i]))+comind
    ratio=float(ratio_mem)/(1.0+comind)
    q=cdiv(cat[0],50); R=float(q)+comind
    ind=float(cat[2]); p50=float(new10+50000)
    X=((p50*K1)*ratio)*ind
    Y=(float(s16(c9200))*K2+ECON[econ])*ratio
    Z=ind*Y
    if Z<K3: Z=15.0
    cen=lambda i: census.get(i,0)
    l1=(cdiv(cen(0xD5),3)+cen(0xD7)+cen(0xDA)+cen(0xF8)+10)*150
    if float(l1)<R: R=float(l1)
    l2=cat[1]*4+500
    if float(l2)<R: R=float(l2)
    q3=cdiv(cen(0xDD)+cen(0xDE)+cb4f0,5); l3=(q3*4+4)*375
    if float(l3)<X: X=float(l3)
    l4=((cen(0xE0)+c9408+1)*4)*375
    if float(l4)<Z: Z=float(l4)
    D=[R,X,Z]; E=[D[i]/float(cat[i]+1)-K0 for i in range(3)]
    out=[]
    for i in range(3):
        t=tax[i]
        if i==0:
            if ordin&2: t+=1
            if ordin&0x4000: t-=1
        elif i==1:
            if ordin&1: t+=1
            if ordin&0x1000: t-=1
            if ordin&0x8000: t-=1
            if ordin&0x40000: t-=1
        else:
            if ordin&0x2000: t-=1
            if ordin&0x80000: t+=1
        t=max(t,0)
        v=E[i]*K4+float(TAX[t]); delta=int(v)   # trunc toward zero
        d=s16(s16(delta)+dem_old[i]); d=max(-2000,min(2000,d)); out.append(d)
    return D,E,out
if __name__=='__main__':
    census={0xD5:30,0xD7:4,0xDA:2,0xF8:3,0xDD:20,0xDE:10,0xE0:7}
    for name,args in {
        'A': dict(zp=[0,300,2000,100,900,50,700,0], pop10_old=39000, ratio_mem=23000, c9200=40, econ=1, census=census, extra=0, tax=[7,6,7], ordin=0, dem_old=[100,-50,0]),
        'B': dict(zp=[0,10,150,5,40,30,60,0], pop10_old=2800, ratio_mem=1500, c9200=-5, econ=2, census={}, extra=900, tax=[9,9,9], ordin=0x4000|0x1000|0x80000, dem_old=[1990,-1990,3]),
    }.items():
        D,E,out=demand(**args)
        print(name,'D=',repr(D),'E=',repr(E),'demand=',out)
