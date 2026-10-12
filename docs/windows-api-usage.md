# Windows API usage — `SIMCITY.EXE` (1995 CD Collection, build .022)

Status: **import-table analysis only** (VERIFIED: which APIs are imported). *How* each is used is not yet analysed — all "intent" and "SDL3 mapping" columns are HYPOTHESIS until call sites are examined in the disassembly.

Generated tables at the bottom come from `tools/gen_api_usage.py`. Note that SIMCITY.EXE statically links MFC 3.x, so a large fraction of imports (menus, dialog helpers, DC state getters, printing, metafile functions, `Escape`, `StartDoc`, `ArcTo`, …) are almost certainly **MFC runtime plumbing**, not game logic. Only the groups below look game-relevant.

## Game-relevant groups and proposed native mapping

| Area | Imported APIs (VERIFIED) | Likely intent (HYPOTHESIS) | SDL3 / native plan |
|---|---|---|---|
| Palette animation | `CreatePalette`, `SelectPalette`, `RealizePalette`, `AnimatePalette`, `GetPaletteEntries`, `GetSystemPaletteEntries`, `SetSystemPaletteUse`, `GetDIBColorTable`, `SetDIBColorTable`, `GetDeviceCaps` | 256-colour indexed rendering with palette-cycling animation (water/power-plant lights etc.; sc2kfix: cycle ranges 171–219 & 224–239 via `AnimatePalette`) and takeover of the system palette | Keep indexed 8-bit surfaces; implement palette cycling in our own palette table; upload as RGBA each frame (or palette shader). No system-palette mode. |
| Drawing | `CreateDIBSection`, `CreateCompatibleDC/Bitmap`, `BitBlt`, `StretchBlt`, `PatBlt`, `SetStretchBltMode` | Software-composited DIB back buffer blitted to window; `StretchBlt` for zoom | Render into an 8-bit software framebuffer → SDL texture; scaling by renderer |
| Timing | `timeSetEvent`, `timeKillEvent`, `timeBeginPeriod`, `timeEndPeriod`, `timeGetTime`, `SetTimer`, `KillTimer`, `GetTickCount` | Multimedia timer drives simulation/animation ticks (string: "Unable to start the timer. SimCity 2000 cities won't grow.") | Fixed-step accumulator; period to be read from code |
| Music | `mciSendCommandA` (+ string `sequencer`) | MIDI playback through MCI sequencer, files `\%d.MID` | Parse SMF → FluidSynth |
| SFX | `sndPlaySoundA` | Plays `\%d.WAV` (maybe from memory) | SDL3 audio streams, mixing our own |
| Files | `CreateFileA`, `ReadFile`, `WriteFile`, `SetFilePointer`, `_lopen/_lread/_hread/_llseek`, `FindFirstFileA`, `GetOpenFileNameA`, `GetSaveFileNameA`, `GetFileTitleA`, `GetPrivateProfile*`, `WritePrivateProfileStringA` | Loads data archives, saves cities, file dialogs, INI-style settings | `std::filesystem`/stdio + macOS open/save panels (SDL3 file dialogs) |
| Settings | `RegCreateKeyExA`, `RegOpenKeyExA`, `RegQueryValueExA`, `RegSetValueExA`, `RegDeleteKeyA`, … (strings `Software`, `Maxis`, `SC2K`, `REGISTRATION` exist; full key path not yet reconstructed) | Persist options & registration | JSON/plist preferences in Application Support |
| Memory | `VirtualAlloc/Free`, `GlobalAlloc/Lock/Free`, `LocalAlloc/Free` | Large map/array allocations (`VirtualAlloc`) | `std::vector` / aligned buffers; layout-sensitive structures stay byte-exact |
| Threads | `CreateThread`, `ExitThread`, critical sections, TLS | `CreateThread` is imported (may be CRT `_beginthread` or game code; unexamined) | Single-threaded sim; see journal for follow-up |
| Input | `GetAsyncKeyState`, `GetKeyState`, `GetCursorPos`, `SetCapture`, `ReleaseCapture`, `LoadCursorA`, `SetCursor`, `ShowCursor`, `TranslateAcceleratorA` | Mouse tool interaction, scrolling, custom cursors (101 cursor resources) | SDL3 events; custom cursors from resources |
| Printing | `OpenPrinterA`, `DocumentPropertiesA`, `StartDocA`, `StartPage`, `EndPage`, `AbortDoc` | Print support (MFC `CView::OnPrint`) | Out of scope initially |
| Drag & drop / shell | `DragAcceptFiles`, `DragQueryFileA`, `DragFinish` | Open city by dropping file | `SDL_EVENT_DROP_FILE` |
| Common controls | `ImageList_Create/LoadImage/Merge/Read/Write/Destroy` (COMCTL32) | Toolbar/status bar image lists (MFC) | Own UI rendering |
| Help | `WinHelpA` (string) | `.HLP` help | Out of scope (WinHelp format); consider text export later |

Absent (VERIFIED): DirectDraw, DirectSound, WinG, `waveOut*`/`midiOut*` (MCI is used instead), `QueryPerformanceCounter`, `GetPrivateProfile` for sounds, MFC DLLs. Consequently all graphics are GDI-based and sound is MCI/`sndPlaySound`.

## Full import tables
### SIMCITY.EXE (by DLL; categories are a keyword heuristic)

### WINMM.dll (7 imports)

- **Audio / multimedia** (2): `mciSendCommandA`, `sndPlaySoundA`
- **Timers / timing** (5): `timeBeginPeriod`, `timeEndPeriod`, `timeGetTime`, `timeKillEvent`, `timeSetEvent`

### KERNEL32.dll (109 imports)

- **Files / paths** (24): `CloseHandle`, `CreateFileA`, `DeleteFileA`, `FindClose`, `FindFirstFileA`, `FindNextFileA`, `GetCurrentDirectoryA`, `GetFileAttributesA`, `GetFileSize`, `GetFullPathNameA`, `GetModuleFileNameA`, `GetPrivateProfileIntA`, `GetPrivateProfileStringA`, `MoveFileA`, `ReadFile`, `SetCurrentDirectoryA`, `SetEndOfFile`, `SetFilePointer`, `WriteFile`, `WritePrivateProfileStringA`, `_lclose`, `_llseek`, `_lopen`, `_lread`
- **Memory** (9): `GlobalAlloc`, `GlobalFree`, `GlobalLock`, `GlobalSize`, `GlobalUnlock`, `LocalAlloc`, `LocalFree`, `VirtualAlloc`, `VirtualFree`
- **Threads / sync** (10): `CreateThread`, `DeleteCriticalSection`, `EnterCriticalSection`, `ExitThread`, `InitializeCriticalSection`, `LeaveCriticalSection`, `TlsAlloc`, `TlsFree`, `TlsGetValue`, `TlsSetValue`
- **Timers / timing** (1): `GetTickCount`
- **Other** (65): `DuplicateHandle`, `ExitProcess`, `FatalAppExitA`, `FileTimeToLocalFileTime`, `FileTimeToSystemTime`, `FindResourceA`, `FlushFileBuffers`, `FreeLibrary`, `FreeResource`, `GetACP`, `GetCPInfo`, `GetCommandLineA`, `GetCurrentProcess`, `GetCurrentThread`, `GetCurrentThreadId`, `GetEnvironmentStrings`, `GetFileTime`, `GetFileType`, `GetLastError`, `GetLocalTime`, `GetLogicalDrives`, `GetModuleHandleA`, `GetOEMCP`, `GetProcAddress`, `GetShortPathNameA`, `GetStartupInfoA`, `GetStdHandle`, `GetTimeZoneInformation`, `GetVersion`, `GetVolumeInformationA`, `GlobalAddAtomA`, `GlobalDeleteAtom`, `GlobalGetAtomNameA`, `IsBadCodePtr`, `IsBadReadPtr`, `IsBadWritePtr`, `LoadLibraryA`, `LoadResource`, `LocalFileTimeToFileTime`, `LockFile`, `LockResource`, `MulDiv`, `MultiByteToWideChar`, `RaiseException`, `ResumeThread`, `RtlUnwind`, `SetConsoleCtrlHandler`, `SetEnvironmentVariableA`, `SetErrorMode`, `SetFileAttributesA`, `SetFileTime`, `SetLastError`, `SetStdHandle`, `SetThreadPriority`, `SetUnhandledExceptionFilter`, `SystemTimeToFileTime`, `UnhandledExceptionFilter`, `UnlockFile`, `WideCharToMultiByte`, `_hread`, `lstrcatA`, `lstrcmpA`, `lstrcmpiA`, `lstrcpyA`, `lstrlenA`

### USER32.dll (176 imports)

- **Bitmap / blit / DIB** (1): `LoadBitmapA`
- **Input** (10): `GetAsyncKeyState`, `GetCapture`, `GetCursorPos`, `GetKeyState`, `LoadCursorA`, `ReleaseCapture`, `SetCapture`, `SetCursor`, `ShowCursor`, `TranslateAcceleratorA`
- **Timers / timing** (2): `KillTimer`, `SetTimer`
- **Windows / menus / dialogs** (40): `BeginPaint`, `CallWindowProcA`, `CheckMenuItem`, `CreateDialogIndirectParamA`, `CreateDialogParamA`, `CreateWindowExA`, `DefWindowProcA`, `DestroyWindow`, `DialogBoxIndirectParamA`, `DialogBoxParamA`, `DispatchMessageA`, `EnableMenuItem`, `EndDialog`, `EndPaint`, `GetDC`, `GetDCEx`, `GetMessageA`, `GetMessagePos`, `GetMessageTime`, `GetWindowLongA`, `InvalidateRect`, `LoadMenuA`, `MessageBoxA`, `ModifyMenuA`, `MoveWindow`, `PeekMessageA`, `PostMessageA`, `RegisterClassA`, `ReleaseDC`, `ScrollWindow`, `SendMessageA`, `SetMenu`, `SetMenuItemBitmaps`, `SetWindowLongA`, `SetWindowPos`, `SetWindowTextA`, `ShowWindow`, `TrackPopupMenu`, `TranslateMessage`, `UpdateWindow`
- **Other** (123): `AdjustWindowRectEx`, `AppendMenuA`, `BeginDeferWindowPos`, `BringWindowToTop`, `CallNextHookEx`, `CharLowerA`, `CharLowerBuffA`, `CharToOemA`, `CharToOemBuffA`, `CharUpperA`, `ChildWindowFromPoint`, `ClientToScreen`, `CopyRect`, `DefFrameProcA`, `DefMDIChildProcA`, `DeferWindowPos`, `DeleteMenu`, `DestroyCursor`, `DestroyMenu`, `DrawFocusRect`, `DrawMenuBar`, `DrawTextA`, `EnableWindow`, `EndDeferWindowPos`, `EqualRect`, `FillRect`, `FlashWindow`, `FrameRect`, `GetActiveWindow`, `GetClassInfoA`, `GetClassNameA`, `GetClientRect`, `GetDesktopWindow`, `GetDialogBaseUnits`, `GetDlgCtrlID`, `GetDlgItem`, `GetDlgItemInt`, `GetFocus`, `GetForegroundWindow`, `GetLastActivePopup`, `GetMenu`, `GetMenuCheckMarkDimensions`, `GetMenuItemCount`, `GetMenuItemID`, `GetMenuState`, `GetMenuStringA`, `GetNextDlgGroupItem`, `GetNextDlgTabItem`, `GetParent`, `GetPropA`, `GetScrollPos`, `GetScrollRange`, `GetSubMenu`, `GetSysColor`, `GetSystemMetrics`, `GetTabbedTextExtentA`, `GetTopWindow`, `GetWindow`, `GetWindowDC`, `GetWindowRect`, `GetWindowTextA`, `GetWindowTextLengthA`, `GetWindowThreadProcessId`, `GrayStringA`, `InflateRect`, `InsertMenuA`, `IntersectRect`, `InvertRect`, `IsCharAlphaNumericA`, `IsCharLowerA`, `IsChild`, `IsDialogMessageA`, `IsDlgButtonChecked`, `IsIconic`, `IsRectEmpty`, `IsWindow`, `IsWindowEnabled`, `IsWindowVisible`, `IsZoomed`, `LoadAcceleratorsA`, `LoadIconA`, `LoadStringA`, `LockWindowUpdate`, `MessageBeep`, `OemToCharA`, `OemToCharBuffA`, `OffsetRect`, `PostQuitMessage`, `PtInRect`, `RedrawWindow`, `RegisterWindowMessageA`, `RemovePropA`, `ReuseDDElParam`, `ScreenToClient`, `SendDlgItemMessageA`, `SetActiveWindow`, `SetDlgItemInt`, `SetDlgItemTextA`, `SetFocus`, `SetForegroundWindow`, `SetMessageQueue`, `SetParent`, `SetPropA`, `SetRect`, `SetRectEmpty`, `SetScrollPos`, `SetScrollRange`, `SetWindowsHookExA`, `ShowCaret`, `ShowOwnedPopups`, `ShowScrollBar`, `SystemParametersInfoA`, `TabbedTextOutA`, `TranslateMDISysAccel`, `UnhookWindowsHookEx`, `UnionRect`, `UnpackDDElParam`, `ValidateRect`, `WaitMessage`, `WinHelpA`, `WindowFromPoint`, `wsprintfA`, `wvsprintfA`

### GDI32.dll (109 imports)

- **Bitmap / blit / DIB** (10): `BitBlt`, `CreateBitmap`, `CreateCompatibleBitmap`, `CreateCompatibleDC`, `CreateDIBSection`, `GetStretchBltMode`, `PatBlt`, `SelectObject`, `SetStretchBltMode`, `StretchBlt`
- **Palette / color animation** (10): `AnimatePalette`, `CreatePalette`, `GetDIBColorTable`, `GetDeviceCaps`, `GetPaletteEntries`, `GetSystemPaletteEntries`, `RealizePalette`, `SelectPalette`, `SetDIBColorTable`, `SetSystemPaletteUse`
- **Other** (89): `AbortDoc`, `ArcTo`, `CombineRgn`, `CreateDCA`, `CreateFontA`, `CreateFontIndirectA`, `CreateHatchBrush`, `CreatePatternBrush`, `CreatePen`, `CreateRectRgn`, `CreateRectRgnIndirect`, `CreateSolidBrush`, `DPtoLP`, `DeleteDC`, `DeleteObject`, `EndDoc`, `EndPage`, `EnumMetaFile`, `Escape`, `ExcludeClipRect`, `ExtCreatePen`, `ExtSelectClipRgn`, `ExtTextOutA`, `GetBkColor`, `GetBkMode`, `GetCharWidthA`, `GetClipBox`, `GetClipRgn`, `GetCurrentObject`, `GetCurrentPositionEx`, `GetMapMode`, `GetNearestColor`, `GetObjectA`, `GetObjectType`, `GetPolyFillMode`, `GetROP2`, `GetStockObject`, `GetTextAlign`, `GetTextColor`, `GetTextExtentPointA`, `GetTextFaceA`, `GetTextMetricsA`, `GetViewportExtEx`, `GetViewportOrgEx`, `GetWindowExtEx`, `GetWindowOrgEx`, `IntersectClipRect`, `LPtoDP`, `LineTo`, `MoveToEx`, `OffsetClipRgn`, `OffsetViewportOrgEx`, `OffsetWindowOrgEx`, `PlayMetaFile`, `PlayMetaFileRecord`, `PolyBezierTo`, `PolyDraw`, `PolylineTo`, `PtVisible`, `RectVisible`, `Rectangle`, `RestoreDC`, `SaveDC`, `ScaleViewportExtEx`, `ScaleWindowExtEx`, `SelectClipPath`, `SelectClipRgn`, `SetAbortProc`, `SetArcDirection`, `SetBkColor`, `SetBkMode`, `SetBrushOrgEx`, `SetColorAdjustment`, `SetMapMode`, `SetMapperFlags`, `SetPolyFillMode`, `SetROP2`, `SetRectRgn`, `SetTextAlign`, `SetTextCharacterExtra`, `SetTextColor`, `SetTextJustification`, `SetViewportExtEx`, `SetViewportOrgEx`, `SetWindowExtEx`, `SetWindowOrgEx`, `StartDocA`, `StartPage`, `TextOutA`

### comdlg32.dll (5 imports)

- **Files / paths** (3): `GetFileTitleA`, `GetOpenFileNameA`, `GetSaveFileNameA`
- **Other** (2): `CommDlgExtendedError`, `PrintDlgA`

### WINSPOOL.DRV (3 imports)

- **Other** (3): `ClosePrinter`, `DocumentPropertiesA`, `OpenPrinterA`

### ADVAPI32.dll (9 imports)

- **Registry / settings** (9): `RegCloseKey`, `RegCreateKeyExA`, `RegDeleteKeyA`, `RegDeleteValueA`, `RegOpenKeyExA`, `RegQueryValueA`, `RegQueryValueExA`, `RegSetValueA`, `RegSetValueExA`

### SHELL32.dll (3 imports)

- **Other** (3): `DragAcceptFiles`, `DragFinish`, `DragQueryFileA`

### COMCTL32.dll (7 imports)

- **Bitmap / blit / DIB** (6): `ImageList_Create`, `ImageList_Destroy`, `ImageList_LoadImageA`, `ImageList_Merge`, `ImageList_Read`, `ImageList_Write`
- **Other** (1): `#17`

