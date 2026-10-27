#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

static int fail(const wchar_t *message) {
    MessageBoxW(NULL, message, L"Android APK 빌더", MB_OK | MB_ICONERROR);
    return 1;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR arguments, int show) {
    (void)instance; (void)previous; (void)arguments; (void)show;
    wchar_t root[32768], script[32768], python[32768], command[65536];
    DWORD length = GetModuleFileNameW(NULL, root, (DWORD)(sizeof(root) / sizeof(root[0])));
    if (!length || length >= sizeof(root) / sizeof(root[0]))
        return fail(L"실행 파일의 위치를 확인할 수 없습니다.");
    wchar_t *part = wcsrchr(root, L'\\');
    if (!part) return fail(L"실행 파일의 위치가 올바르지 않습니다.");
    *part = L'\0';
    part = wcsrchr(root, L'\\');
    if (!part) return fail(L"프로젝트 폴더를 찾을 수 없습니다.");
    *part = L'\0';
    if (_snwprintf_s(script, _countof(script), _TRUNCATE, L"%ls\\android\\builder\\gui.py", root) < 0)
        return fail(L"GUI 파일 경로가 너무 깁니다.");
    DWORD attributes = GetFileAttributesW(script);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
        return fail(L"GUI 파일을 찾을 수 없습니다. 실행 파일을 원래 작업공간의 build 폴더에 두세요.");

    const wchar_t *path = _wgetenv(L"PATH");
    if (!path || !*path) return fail(L"PATH에서 Python 3의 pythonw.exe를 찾을 수 없습니다.");
    length = SearchPathW(path, L"pythonw.exe", NULL, (DWORD)_countof(python), python, NULL);
    if (!length || length >= _countof(python))
        return fail(L"PATH에서 Python 3의 pythonw.exe를 찾을 수 없습니다. Python과 tkinter를 설치하세요.");

    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process = {0};
    if (_snwprintf_s(command, _countof(command), _TRUNCATE,
                     L"\"%ls\" -X utf8 -c \"import sys, tkinter; sys.exit(0 if sys.version_info >= (3, 9) else 1)\"", python) < 0)
        return fail(L"Python 실행 경로가 너무 깁니다.");
    if (!CreateProcessW(python, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, root, &startup, &process))
        return fail(L"Python을 실행할 수 없습니다. Python 3.9 이상과 tkinter를 확인하세요.");
    DWORD waited = WaitForSingleObject(process.hProcess, 10000), code = 1;
    if (waited == WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (waited != WAIT_OBJECT_0 || code != 0)
        return fail(L"Python 3.9 이상과 tkinter를 사용할 수 없습니다. Python 설치를 확인하세요.");

    if (_snwprintf_s(command, _countof(command), _TRUNCATE, L"\"%ls\" -X utf8 \"%ls\"", python, script) < 0)
        return fail(L"GUI 실행 경로가 너무 깁니다.");
    ZeroMemory(&process, sizeof(process));
    if (!CreateProcessW(python, command, NULL, NULL, FALSE, 0, NULL, root, &startup, &process))
        return fail(L"Android APK 빌더를 시작할 수 없습니다.");
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}
