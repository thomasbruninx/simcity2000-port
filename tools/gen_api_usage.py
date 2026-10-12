#!/usr/bin/env python3
"""Generate the per-DLL import tables for docs/windows-api-usage.md. Usage: gen_api_usage.py <exe> [<exe>...]"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe_inspect import PE
CATS = [
 ('Palette / color animation', ('Palette','RealizePalette','AnimatePalette','SystemPalette','SetSystemPaletteUse','GetNearestPaletteIndex','GetDIBColorTable','SetDIBColorTable','GetDeviceCaps')),
 ('Bitmap / blit / DIB', ('BitBlt','StretchBlt','StretchDIBits','SetDIBits','GetDIBits','CreateDIB','CreateBitmap','CreateCompatible','SelectObject','PatBlt','MaskBlt','SetStretchBltMode','DrawBitmap','LoadBitmap','LoadImage','ImageList')),
 ('Timers / timing', ('timeSetEvent','timeKillEvent','timeBeginPeriod','timeEndPeriod','timeGetTime','SetTimer','KillTimer','GetTickCount','Sleep','QueryPerformance')),
 ('Audio / multimedia', ('mci','sndPlaySound','midi','wave','PlaySound')),
 ('Memory', ('VirtualAlloc','VirtualFree','GlobalAlloc','GlobalLock','GlobalFree','GlobalUnlock','GlobalSize','LocalAlloc','LocalFree','HeapAlloc','HeapFree','HeapCreate','GlobalReAlloc','LocalReAlloc')),
 ('Files / paths', ('CreateFile','ReadFile','WriteFile','SetFilePointer','CloseHandle','FindFirst','FindNext','FindClose','GetFileAttributes','GetModuleFileName','GetCurrentDirectory','SetCurrentDirectory','GetOpenFileName','GetSaveFileName','GetFileTitle','DeleteFile','MoveFile','CopyFile','GetFullPathName','GetTempPath','GetWindowsDirectory','GetPrivateProfile','WritePrivateProfile','OpenFile','_lopen','_lread','_lclose','_llseek','_lwrite','_lcreat','GetFileSize','SetEndOfFile')),
 ('Registry / settings', ('RegOpenKey','RegCreateKey','RegQuery','RegSet','RegClose','RegDelete')),
 ('Threads / sync', ('CreateThread','ExitThread','CreateEvent','SetEvent','WaitFor','CriticalSection','Interlocked','TlsAlloc','TlsGetValue','TlsSetValue','TlsFree','CreateMutex','ReleaseMutex')),
 ('Input', ('GetKeyState','GetAsyncKeyState','GetCursorPos','SetCursorPos','SetCapture','ReleaseCapture','ClipCursor','GetCapture','ShowCursor','LoadCursor','SetCursor','TranslateAccelerator','GetKeyboardState','MapVirtualKey','VkKeyScan','ToAscii')),
 ('Windows / menus / dialogs', ('CreateWindow','DestroyWindow','ShowWindow','MoveWindow','SetWindowPos','DialogBox','CreateDialog','EndDialog','MessageBox','CreateMenu','LoadMenu','TrackPopupMenu','EnableMenuItem','CheckMenuItem','ModifyMenu','SetMenu','GetMessage','PeekMessage','DispatchMessage','TranslateMessage','PostMessage','SendMessage','DefWindowProc','CallWindowProc','SetWindowLong','GetWindowLong','RegisterClass','SetWindowText','InvalidateRect','UpdateWindow','BeginPaint','EndPaint','ScrollWindow','GetDC','ReleaseDC')),
]
def cat(f):
    for name, keys in CATS:
        if any(f.startswith(k) or k in f for k in keys): return name
    return None
for path in sys.argv[1:]:
    pe = PE(path)
    print('## %s\n' % os.path.basename(path))
    for dll, fs in pe.imports():
        print('### %s (%d imports)\n' % (dll, len(fs)))
        groups = {}
        for f in fs: groups.setdefault(cat(f) or 'Other', []).append(f)
        for g, l in sorted(groups.items(), key=lambda kv: (kv[0]=='Other', kv[0])):
            print('- **%s** (%d): `%s`' % (g, len(l), '`, `'.join(sorted(l))))
        print()
