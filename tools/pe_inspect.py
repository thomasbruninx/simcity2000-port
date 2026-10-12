#!/usr/bin/env python3
"""Minimal dependency-free PE32 inspector (read-only).

Usage: python3 -I tools/pe_inspect.py <file.exe|dll> [--resources] [--imports] [--json]

Reports: DOS/COFF/optional headers, sections (with entropy), imports, exports,
resource tree (type/name/lang, size), VS_VERSION_INFO strings, debug directory,
Rich header presence, relocation summary. Everything printed is parsed directly
from the file; nothing is inferred.
"""
import struct, sys, math, hashlib, datetime, json

RT = {1:'CURSOR',2:'BITMAP',3:'ICON',4:'MENU',5:'DIALOG',6:'STRING',7:'FONTDIR',8:'FONT',
      9:'ACCELERATOR',10:'RCDATA',11:'MESSAGETABLE',12:'GROUP_CURSOR',14:'GROUP_ICON',
      16:'VERSION',17:'DLGINCLUDE',19:'PLUGPLAY',20:'VXD',21:'ANICURSOR',22:'ANIICON',
      23:'HTML',24:'MANIFEST'}

def u16(d,o): return struct.unpack_from('<H',d,o)[0]
def u32(d,o): return struct.unpack_from('<I',d,o)[0]

def entropy(b):
    if not b: return 0.0
    c=[0]*256
    for x in b: c[x]+=1
    n=len(b)
    return -sum(v/n*math.log2(v/n) for v in c if v)

class PE:
    def __init__(self, path):
        self.path=path
        self.d=open(path,'rb').read()
        d=self.d
        if d[:2]!=b'MZ': raise ValueError('not MZ')
        self.e_lfanew=u32(d,0x3c)
        if d[self.e_lfanew:self.e_lfanew+4]!=b'PE\0\0': raise ValueError('no PE sig (NE/LE?)')
        o=self.e_lfanew+4
        (self.machine,self.nsec,self.timestamp,self.symptr,self.nsyms,self.optsize,self.chars)=struct.unpack_from('<HHIIIHH',d,o)
        o+=20
        self.opt_off=o
        self.magic=u16(d,o)
        if self.magic!=0x10b: raise ValueError('only PE32 supported, magic=%x'%self.magic)
        (self.linker_major,self.linker_minor,self.size_code,self.size_idata,self.size_udata,
         self.entry_rva,self.base_code,self.base_data,self.image_base,self.sec_align,self.file_align,
         self.os_maj,self.os_min,self.img_maj,self.img_min,self.sub_maj,self.sub_min,self.win32ver,
         self.size_image,self.size_headers,self.checksum,self.subsystem,self.dllchars,
         self.stack_res,self.stack_commit,self.heap_res,self.heap_commit,self.loaderflags,self.ndd)=\
            struct.unpack_from('<BBIIIIIIIIIHHHHHHIIIIHHIIIIII',d,o+2)
        self.dd=[struct.unpack_from('<II',d,o+96+8*i) for i in range(self.ndd)]
        so=o+self.optsize
        self.sections=[]
        for i in range(self.nsec):
            n,vs,va,rs,rp,_,_,_,_,ch=struct.unpack_from('<8sIIIIIIHHI',d,so+40*i)
            self.sections.append(dict(name=n.rstrip(b'\0').decode('latin1'),vsize=vs,rva=va,rawsize=rs,rawptr=rp,chars=ch))
    def rva2off(self,rva):
        for s in self.sections:
            if s['rva']<=rva<s['rva']+max(s['vsize'],s['rawsize']):
                return rva-s['rva']+s['rawptr']
        if rva<self.size_headers: return rva
        return None
    def cstr(self,off):
        e=self.d.index(b'\0',off); return self.d[off:e].decode('latin1')
    def imports(self):
        rva,size=self.dd[1]; out=[]
        if not rva: return out
        o=self.rva2off(rva)
        while True:
            oft,_,_,name,ft=struct.unpack_from('<IIIII',self.d,o)
            if not name: break
            dll=self.cstr(self.rva2off(name)); funcs=[]
            t=self.rva2off(oft or ft)
            while True:
                v=u32(self.d,t)
                if not v: break
                if v&0x80000000: funcs.append('#%d'%(v&0xffff))
                else: funcs.append(self.cstr(self.rva2off(v)+2))
                t+=4
            out.append((dll,funcs)); o+=20
        return out
    def exports(self):
        rva,size=self.dd[0]
        if not rva: return None
        o=self.rva2off(rva)
        (_,ts,_,_,namerva,base,nfun,nnames,afun,anames,aords)=struct.unpack_from('<IIHHIIIIIII',self.d,o)
        res=dict(dllname=self.cstr(self.rva2off(namerva)),base=base,funcs=[])
        fo=self.rva2off(afun); no=self.rva2off(anames); oo=self.rva2off(aords)
        names={}
        for i in range(nnames):
            nm=self.cstr(self.rva2off(u32(self.d,no+4*i))); names[u16(self.d,oo+2*i)]=nm
        for i in range(nfun):
            res['funcs'].append((base+i,u32(self.d,fo+4*i),names.get(i)))
        return res
    def resources(self):
        rva,size=self.dd[2]
        if not rva: return []
        base=self.rva2off(rva); out=[]
        def name(v):
            if v&0x80000000:
                o=base+(v&0x7fffffff); n=u16(self.d,o)
                return self.d[o+2:o+2+2*n].decode('utf-16le')
            return v
        def walk(off,level,path):
            _,_,_,_,nn,ni=struct.unpack_from('<IIHHHH',self.d,off)
            for i in range(nn+ni):
                nid,ofs=struct.unpack_from('<II',self.d,off+16+8*i)
                p=path+[name(nid)]
                if ofs&0x80000000: walk(base+(ofs&0x7fffffff),level+1,p)
                else:
                    drva,dsz,cp,_=struct.unpack_from('<IIII',self.d,base+ofs)
                    out.append((p,drva,dsz))
        walk(base,0,[])
        return out
    def version_strings(self):
        for p,rva,sz in self.resources():
            if p[0]==16:
                b=self.d[self.rva2off(rva):self.rva2off(rva)+sz]
                # crude: extract UTF-16 key/value pairs
                res={}
                import re
                s=b.decode('utf-16le','ignore')
                for m in re.finditer(r'([A-Za-z]+)\x00+([^\x00]{1,200})\x00',s):
                    res[m.group(1)]=m.group(2)
                ffi=b.find(struct.pack('<I',0xFEEF04BD))
                fv=None
                if ffi>=0:
                    _,_,fvms,fvls,pvms,pvls=struct.unpack_from('<IIIIII',b,ffi)
                    fv=(fvms>>16,fvms&0xffff,fvls>>16,fvls&0xffff)
                return fv,res
        return None,{}

def main():
    a=sys.argv[1:]
    if not a: print(__doc__); return 1
    pe=PE(a[0]); d=pe.d
    print('File:',a[0],len(d),'bytes')
    print('SHA-256:',hashlib.sha256(d).hexdigest())
    print('MD5    :',hashlib.md5(d).hexdigest())
    mach={0x14c:'i386'}.get(pe.machine,hex(pe.machine))
    print('Machine:',mach,' Characteristics: 0x%04x'%pe.chars,
          '(DLL)' if pe.chars&0x2000 else '(EXE)', '(relocs stripped)' if pe.chars&1 else '')
    print('TimeDateStamp: 0x%08x = %s UTC'%(pe.timestamp,datetime.datetime.fromtimestamp(pe.timestamp,datetime.UTC)))
    print('Linker: %d.%d  OS %d.%d  Subsys ver %d.%d  Subsystem %d'%(pe.linker_major,pe.linker_minor,pe.os_maj,pe.os_min,pe.sub_maj,pe.sub_min,pe.subsystem))
    print('ImageBase 0x%08x  EntryRVA 0x%x  (VA 0x%08x)  SizeOfImage 0x%x  Align %x/%x  Checksum 0x%x'%(pe.image_base,pe.entry_rva,pe.image_base+pe.entry_rva,pe.size_image,pe.sec_align,pe.file_align,pe.checksum))
    print('Stack res/commit 0x%x/0x%x  Heap res/commit 0x%x/0x%x'%(pe.stack_res,pe.stack_commit,pe.heap_res,pe.heap_commit))
    rich=d.find(b'Rich',0x80,pe.e_lfanew)
    print('Rich header:','present' if rich>0 else 'absent')
    names=['Export','Import','Resource','Exception','Security','BaseReloc','Debug','Arch','GlobalPtr','TLS','LoadCfg','BoundImp','IAT','DelayImp','COM','Reserved']
    print('Data directories:')
    for i,(r,s) in enumerate(pe.dd):
        if r or s: print('  %-10s RVA 0x%08x size 0x%x'%(names[i] if i<16 else i,r,s))
    print('Sections:')
    for s in pe.sections:
        raw=d[s['rawptr']:s['rawptr']+s['rawsize']]
        print('  %-8s RVA 0x%08x vsize 0x%08x raw 0x%08x @0x%08x flags 0x%08x entropy %.2f'%(s['name'],s['rva'],s['vsize'],s['rawsize'],s['rawptr'],s['chars'],entropy(raw)))
    ep=[s['name'] for s in pe.sections if s['rva']<=pe.entry_rva<s['rva']+s['vsize']]
    print('Entry point in section:',ep)
    # debug dir
    r,s=pe.dd[6]
    if r:
        o=pe.rva2off(r)
        for i in range(s//28):
            ch,ts,ma,mi,ty,sz,ra,rp=struct.unpack_from('<IIHHIIII',d,o+28*i)
            print('Debug entry: type',ty,'size',sz,'rawptr 0x%x'%rp, d[rp:rp+4])
    # relocs
    r,s=pe.dd[5]
    if r:
        o=pe.rva2off(r); end=o+s; n=0; p=o
        while p<end:
            pg,bs=struct.unpack_from('<II',d,p)
            if bs==0: break
            n+=(bs-8)//2; p+=bs
        print('Base relocations: %d entries'%n)
    ex=pe.exports()
    if ex:
        print('Exports (%s): %d'%(ex['dllname'],len(ex['funcs'])))
        for ordn,rva,nm in ex['funcs'][:400]: print('  #%d 0x%08x %s'%(ordn,rva,nm))
    fv,vs=pe.version_strings()
    if fv or vs:
        print('Version info: fileversion',fv)
        for k,v in vs.items(): print('  ',k,'=',v)
    if '--imports' in a or '--all' in a or True:
        print('Imports:')
        for dll,fs in pe.imports():
            print('  %s (%d)'%(dll,len(fs)))
            if '--imports' in a:
                for f in fs: print('     ',f)
    res=pe.resources()
    from collections import Counter
    c=Counter((RT.get(p[0],p[0]) if isinstance(p[0],int) else 'named:'+str(p[0])) for p,_,_ in res)
    print('Resources: %d leaf entries'%len(res), dict(c))
    if '--resources' in a:
        for p,rva,sz in res:
            t=RT.get(p[0],p[0]) if isinstance(p[0],int) else p[0]
            print('  type=%s name=%s lang=%s  rva=0x%x size=%d'%(t,p[1] if len(p)>1 else '',p[2] if len(p)>2 else '',rva,sz))
    return 0
if __name__=='__main__': sys.exit(main())
