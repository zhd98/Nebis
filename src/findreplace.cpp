#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

#include "editor.h"
#include "Scintilla.h"
#include <commctrl.h>
#include <string>

static const wchar_t FR_CLASS[] = L"MdPadFR";

static HWND g_frSci = nullptr;     // active editor
static HWND g_frDlg = nullptr;     // our window (so we don't open twice)
static HWND g_hFind = nullptr;
static HWND g_hReplace = nullptr;
static HWND g_hCase = nullptr;
static HWND g_hWord = nullptr;
static bool g_replace = false;

#define ID_FIND_EDIT    1
#define ID_REPLACE_EDIT  2
#define ID_CASE          3
#define ID_WORD          4
#define ID_FINDNEXT      5
#define ID_REPLACE_BTN   6
#define ID_REPLACEALL    7
#define ID_CLOSE         8

// Convert a wide string (from an edit control) to UTF-8 (what Scintilla stores).
static std::string WToUTF8(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    if (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}

static std::string GetEditUtf8(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring w(n + 1, L'\0');
    GetWindowTextW(h, w.data(), n + 1);
    return WToUTF8(w);
}

static int GetFlags() {
    int f = 0;
    // Note: Button_GetCheck() macro exists in MSVC SDK but NOT in MinGW-w64
    // headers, so send BM_GETCHECK directly.
    if (SendMessageW(g_hCase, BM_GETCHECK, 0, 0) == BST_CHECKED) f |= SCFIND_MATCHCASE;
    if (SendMessageW(g_hWord, BM_GETCHECK, 0, 0) == BST_CHECKED) f |= SCFIND_WHOLEWORD;
    return f;
}

static void DoFind() {
    if (!g_frSci) return;
    HWND sci = g_frSci;
    std::string find = GetEditUtf8(g_hFind);
    if (find.empty()) return;

    long len = (long)SendMessageW(sci, SCI_GETLENGTH, 0, 0);
    SendMessageW(sci, SCI_SETSEARCHFLAGS, GetFlags(), 0);

    long start = (long)SendMessageW(sci, SCI_GETCURRENTPOS, 0, 0);
    SendMessageW(sci, SCI_SETTARGETRANGE, start, len);
    long pos = (long)SendMessageW(sci, SCI_SEARCHINTARGET, -1, (LPARAM)find.c_str());
    if (pos < 0) { // wrap around from the top
        SendMessageW(sci, SCI_SETTARGETRANGE, 0, start);
        pos = (long)SendMessageW(sci, SCI_SEARCHINTARGET, -1, (LPARAM)find.c_str());
    }
    if (pos < 0) {
        MessageBoxW(g_frDlg, L"找不到匹配项。", L"查找", MB_OK | MB_ICONINFORMATION);
        return;
    }
    long ts = (long)SendMessageW(sci, SCI_GETTARGETSTART, 0, 0);
    long te = (long)SendMessageW(sci, SCI_GETTARGETEND, 0, 0);
    SendMessageW(sci, SCI_SETSEL, ts, te);
}

static void DoReplace() {
    if (!g_frSci) return;
    HWND sci = g_frSci;
    std::string find = GetEditUtf8(g_hFind);
    std::string repl = GetEditUtf8(g_hReplace);
    if (find.empty()) return;

    long ts = (long)SendMessageW(sci, SCI_GETTARGETSTART, 0, 0);
    long te = (long)SendMessageW(sci, SCI_GETTARGETEND, 0, 0);
    long ss = (long)SendMessageW(sci, SCI_GETSELECTIONSTART, 0, 0);
    long se = (long)SendMessageW(sci, SCI_GETSELECTIONEND, 0, 0);
    if (ss == ts && se == te && ss != se) { // current selection == last match
        SendMessageW(sci, SCI_REPLACETARGET, -1, (LPARAM)repl.c_str());
    }
    DoFind();
}

static void DoReplaceAll() {
    if (!g_frSci) return;
    HWND sci = g_frSci;
    std::string find = GetEditUtf8(g_hFind);
    std::string repl = GetEditUtf8(g_hReplace);
    if (find.empty()) return;

    int flags = GetFlags();
    SendMessageW(sci, SCI_SETSEARCHFLAGS, flags, 0);
    long len = (long)SendMessageW(sci, SCI_GETLENGTH, 0, 0);
    long pos = 0;
    int count = 0;
    while (true) {
        SendMessageW(sci, SCI_SETTARGETRANGE, pos, len);
        long f = (long)SendMessageW(sci, SCI_SEARCHINTARGET, -1, (LPARAM)find.c_str());
        if (f < 0) break;
        SendMessageW(sci, SCI_REPLACETARGET, -1, (LPARAM)repl.c_str());
        long newLen = (long)SendMessageW(sci, SCI_GETLENGTH, 0, 0);
        pos = f + (long)repl.size();
        if (pos > newLen) break;
        len = newLen;
        ++count;
    }
    std::wstring msg = L"已替换 " + std::to_wstring(count) + L" 处。";
    MessageBoxW(g_frDlg, msg.c_str(), L"替换", MB_OK | MB_ICONINFORMATION);
}

static LRESULT CALLBACK FrWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        int x = 12, y = 12, w = 250;
        CreateWindowExW(0, L"Static", L"查找:", WS_CHILD | WS_VISIBLE, x, y, 48, 22, hwnd, 0, 0, 0);
        g_hFind = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                  64, y, w, 22, hwnd, (HMENU)ID_FIND_EDIT, 0, 0);
        y += 28;
        if (g_replace) {
            CreateWindowExW(0, L"Static", L"替换:", WS_CHILD | WS_VISIBLE, x, y, 48, 22, hwnd, 0, 0, 0);
            g_hReplace = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                         64, y, w, 22, hwnd, (HMENU)ID_REPLACE_EDIT, 0, 0);
            y += 28;
        }
        g_hCase = CreateWindowExW(0, L"Button", L"区分大小写", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                  64, y, 120, 22, hwnd, (HMENU)ID_CASE, 0, 0);
        g_hWord = CreateWindowExW(0, L"Button", L"全词匹配", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                  200, y, 110, 22, hwnd, (HMENU)ID_WORD, 0, 0);
        y += 32;
        int bx = 12;
        CreateWindowExW(0, L"Button", L"查找下一个", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        bx, y, 92, 26, hwnd, (HMENU)ID_FINDNEXT, 0, 0); bx += 100;
        if (g_replace) {
            CreateWindowExW(0, L"Button", L"替换", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                            bx, y, 80, 26, hwnd, (HMENU)ID_REPLACE_BTN, 0, 0); bx += 90;
            CreateWindowExW(0, L"Button", L"全部替换", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                            bx, y, 92, 26, hwnd, (HMENU)ID_REPLACEALL, 0, 0); bx += 100;
        }
        CreateWindowExW(0, L"Button", L"关闭", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        bx, y, 80, 26, hwnd, (HMENU)ID_CLOSE, 0, 0);
        SetFocus(g_hFind);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        switch (id) {
        case ID_FINDNEXT:    DoFind(); break;
        case ID_REPLACE_BTN: DoReplace(); break;
        case ID_REPLACEALL:  DoReplaceAll(); break;
        case ID_CLOSE:       DestroyWindow(hwnd); break;
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_frDlg = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ShowFindReplace(HWND owner, HWND sci, bool replace) {
    if (g_frDlg) { SetForegroundWindow(g_frDlg); return; }

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc; ZeroMemory(&wc, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = FrWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = FR_CLASS;
        RegisterClassExW(&wc);
        registered = true;
    }

    g_replace = replace;
    g_frSci = sci;
    g_frDlg = CreateWindowExW(WS_EX_WINDOWEDGE, FR_CLASS, replace ? L"替换" : L"查找",
                               WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                               CW_USEDEFAULT, CW_USEDEFAULT, 360, replace ? 196 : 168,
                               owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(g_frDlg, SW_SHOW);
    UpdateWindow(g_frDlg);
}
