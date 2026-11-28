from pathlib import Path
from subprocess import run
import sys
from concurrent.futures import ThreadPoolExecutor
def cmd(*args):run(args,check=True)
dlls={'comctl32':'InitCommonControlsEx','winmm':'timeBeginPeriod timeEndPeriod waveOutOpen waveOutPrepareHeader waveOutWrite waveOutReset waveOutPause waveOutRestart waveOutUnprepareHeader waveOutClose','user32':'PeekMessageW MsgWaitForMultipleObjectsEx GetDlgItem SetFocus RegisterRawInputDevices GetRawInputData GetRawInputDeviceInfoW IsDialogMessageW SendMessageW GetSystemMetrics DrawFocusRect KillTimer CreatePopupMenu AppendMenuW DestroyMenu ClientToScreen TrackPopupMenuEx DrawFrameControl SetProcessDPIAware AdjustWindowRect LoadCursorW BeginPaint MessageBoxW PostQuitMessage RegisterClassW CreateWindowExW ShowWindow UpdateWindow GetMessageW TranslateMessage DispatchMessageW DefWindowProcW EndPaint GetClientRect DestroyWindow FillRect SetTimer SetWindowTextW InvalidateRect DrawTextW GetForegroundWindow GetAsyncKeyState GetWindowRect GetMonitorInfoW MonitorFromWindow SetWindowPos SetWindowLongPtrW',
 'kernel32':'GetEnvironmentVariableW CreateThread CreateEventW SetEvent WaitForSingleObject QueryPerformanceCounter QueryPerformanceFrequency GetCurrentProcessId DeleteFileW GetModuleHandleW CreateFileW ReadFile CloseHandle GetFileSize GetLastError MoveFileExW GetTickCount ExitProcess GetModuleFileNameW WriteFile FlushFileBuffers AddVectoredExceptionHandler GetLocalTime LoadLibraryW GetProcAddress FreeLibrary',
 'comdlg32':'GetOpenFileNameW CommDlgExtendedError','gdi32':'IntersectClipRect SetTextCharacterExtra CreateFontW CreateSolidBrush SetStretchBltMode SaveDC RestoreDC ExcludeClipRect CreateCompatibleDC CreateCompatibleBitmap SelectObject DeleteObject DeleteDC BitBlt GetStockObject StretchDIBits SetBkMode SetTextColor', 'xinput9_1_0':'XInputGetState XInputSetState'}
# Create dummy export definitions only to generate import libraries.
for dll,names in dlls.items():
    Path('stubs.s').write_text('.text\n'+''.join(f'.globl {n}\n{n}: ret\n' for n in names.split()))
    cmd(sys.executable,'coff.py','stubs.s','stubs.obj')
    Path(dll+'.def').write_text('LIBRARY '+dll+'.dll\nEXPORTS\n'+names.replace(' ','\n')+'\n')
    cmd('ld','-mi386pep','--dll','--out-implib',dll+'.a',dll+'.def','stubs.obj','-o','stub.dll')
def compile_unit(f):
    if f.startswith('native/'):
        deps=[Path(f+'.c'),Path('cpu_state.h'),Path('lift.inc'),Path('build_win.py'),Path('coff.py')]
        obj=Path(f+'.obj')
        if obj.exists() and obj.stat().st_mtime_ns > max(dep.stat().st_mtime_ns for dep in deps):
            return str(obj)
    cpp = f=='rsp/driver'
    language = ['g++','-S','-std=c++20','-fno-weak','-fno-rtti','-fno-threadsafe-statics','-fno-exceptions'] if cpp else ['gcc','-S','-std=c11']
    cmd(*language,'-DNDEBUG','-fwrapv','-fno-strict-aliasing',('-O1' if f.startswith('native/') else '-O3'),'-msse4.1','-fno-jump-tables','-fno-builtin','-fno-math-errno','-fno-ident','-fPIE','-fshort-wchar','-mabi=ms','-mno-red-zone','-maccumulate-outgoing-args','-fno-stack-protector','-fno-asynchronous-unwind-tables',f+('.cpp' if cpp else '.c'),'-o',f+'.s')
    cmd(sys.executable,'coff.py',f+'.s',f+'.obj')
    return f+'.obj'
units=['runtime','window','output','gpu','musyx/musyx','musyx/audio','rsp/driver']
native=sorted(Path('native').glob('group_*.c'))
if not native:
    raise SystemExit("Run recomp/generate.py with the USA ROM first")
units.extend(str(p.with_suffix('')) for p in native)
with ThreadPoolExecutor(max_workers=6) as pool:
    objects=list(pool.map(compile_unit,units))
cmd('ld','-mi386pep','--no-insert-timestamp','--nxcompat','--stack','0x800000,0x100000','--dynamicbase','--subsystem','windows','-e','start','-o','TowerboundRecomp.exe',*objects,*[dll+'.a' for dll in dlls])
