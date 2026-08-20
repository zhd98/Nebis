#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <cstring>
#include "editor.h"

#define IDM_NEW    40001
#define IDM_OPEN   40002
#define IDM_SAVE   40003
#define IDM_SAVEAS 40004
#define IDM_CLOSE  40005
#define IDM_EXIT   40006
#define IDM_UNDO   40010
#define IDM_REDO   40011
#define IDM_FIND   40012
#define IDM_REPLACE 40013
#define IDM_ABOUT  40020

static const wchar_t FRAME_CLASS[] = L"MdPadFrame";

static HINSTANCE g_hInst = nullptr;
static HWND g_hwndFrame = nullptr;
static HWND g_hwndTab = nullptr;
static std::vector<Document> g_docs;
static int g_active = -1;
static HACCEL g_hAccel = nullptr;

static std::wstring DocBaseName(const std::wstring& path) {
    if (path.empty()) return L"未命名";
    size_t i = path.find_last_of(L"\\/");
    return (i == std::wstring::npos) ? path : path.substr(i + 1);
}
static std::wstring DocTitle(const Document& d) {
    std::wstring t = DocBaseName(d.path);
    if (d.dirty) t += L" *";
    return t;
}
static void UpdateTabText(int idx) {
    if (idx < 0 || idx >= (int)g_docs.size()) return;
    TCITEMW ti; ZeroMemory(&ti, sizeof(ti));
    ti.mask = TCIF_TEXT;
    std::wstring name = DocTitle(g_docs[idx]);
    ti.pszText = (LPWSTR)name.c_str();
    TabCtrl_SetItem(g_hwndTab, idx, &ti);
}
static void UpdateTitle() {
    std::wstring t = (g_active >= 0) ? DocTitle(g_docs[g_active]) : L"MdPad";
    t += L" - MdPad";
    SetWindowTextW(g_hwndFrame, t.c_str());
}
static void Layout() {
    RECT r; GetClientRect(g_hwndFrame, &r);
    int tabH = 26;
    SetWindowPos(g_hwndTab, nullptr, 0, 0, r.right, tabH, SWP_NOZORDER);
    int ex = 0, ey = tabH, ew = r.right, eh = r.bottom - tabH;
    for (auto& d : g_docs)
        SetWindowPos(d.sci, nullptr, ex, ey, ew, eh, SWP_NOZORDER);
}
static void SwitchTo(int idx) {
    if (idx < 0 || idx >= (int)g_docs.size()) return;
    for (size_t i = 0; i < g_docs.size(); i++)
        ShowWindow(g_docs[i].sci, (i == (size_t)idx) ? SW_SHOW : SW_HIDE);
    g_active = idx;
    TabCtrl_SetCurSel(g_hwndTab, idx);
    UpdateTitle();
    Editor::GrabFocus(g_docs[idx].sci);
}
static void NewDoc() {
    HWND sci = Editor::Create(g_hwndFrame, 0, 26, 100, 100);
    if (!sci) return;
    Document d; d.sci = sci; d.path = L""; d.dirty = false;
    g_docs.push_back(d);
    int idx = (int)g_docs.size() - 1;
    g_docs[idx].tab = idx;
    TCITEMW ti; ZeroMemory(&ti, sizeof(ti));
    ti.mask = TCIF_TEXT; ti.pszText = (LPWSTR)L"未命名";
    TabCtrl_InsertItem(g_hwndTab, idx, &ti);
    Layout();
    SwitchTo(idx);
}
static void CloseDoc(int idx) {
    if (idx < 0 || idx >= (int)g_docs.size()) return;
    if (g_docs[idx].dirty &&
        MessageBoxW(g_hwndFrame, L"该文档未保存，确定关闭？", L"MdPad", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    DestroyWindow(g_docs[idx].sci);
    TabCtrl_DeleteItem(g_hwndTab, idx);
    g_docs.erase(g_docs.begin() + idx);
    for (size_t i = 0; i < g_docs.size(); i++) g_docs[i].tab = (int)i;
    if (g_docs.empty()) { NewDoc(); return; }
    int na = (idx < (int)g_docs.size()) ? idx : (int)g_docs.size() - 1;
    Layout(); SwitchTo(na);
}
static void OnOpen() {
    wchar_t file[MAX_PATH] = {0};
    OPENFILENAMEW ofn; ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndFrame;
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Markdown 文件\0*.md;*.markdown;*.mkd\0所有文件\0*.*\0\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;

    int idx;
    if (g_active >= 0 && g_docs[g_active].path.empty() && !g_docs[g_active].dirty
        && Editor::GetLength(g_docs[g_active].sci) == 0) {
        idx = g_active; // reuse the empty untitled tab
    } else {
        NewDoc(); idx = g_active;
    }
    if (Editor::LoadFile(g_docs[idx].sci, file)) {
        g_docs[idx].path = file;
        UpdateTabText(idx); UpdateTitle();
    } else {
        MessageBoxW(g_hwndFrame, L"无法打开文件。", L"错误", MB_OK | MB_ICONERROR);
    }
}
static void OnSave() {
    if (g_active < 0) return;
    if (g_docs[g_active].path.empty()) { OnSaveAs(); return; }
    if (!Editor::SaveFile(g_docs[g_active].sci, g_docs[g_active].path))
        MessageBoxW(g_hwndFrame, L"保存失败。", L"错误", MB_OK | MB_ICONERROR);
}
static void OnSaveAs() {
    if (g_active < 0) return;
    wchar_t file[MAX_PATH] = {0};
    OPENFILENAMEW ofn; ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndFrame;
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Markdown 文件 (*.md)\0*.md\0所有文件 (*.*)\0*.*\0\0";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&ofn)) return;
    if (Editor::SaveFile(g_docs[g_active].sci, file)) {
        g_docs[g_active].path = file;
        UpdateTabText(g_active); UpdateTitle();
    } else {
        MessageBoxW(g_hwndFrame, L"保存失败。", L"错误", MB_OK | MB_ICONERROR);
    }
}

static HMENU CreateMainMenu() {
    HMENU m = CreateMenu();
    HMENU file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, IDM_NEW, L"新建\tCtrl+N");
    AppendMenuW(file, MF_STRING, IDM_OPEN, L"打开...\tCtrl+O");
    AppendMenuW(file, MF_STRING, IDM_SAVE, L"保存\tCtrl+S");
    AppendMenuW(file, MF_STRING, IDM_SAVEAS, L"另存为...");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, IDM_CLOSE, L"关闭标签");
    AppendMenuW(file, MF_STRING, IDM_EXIT, L"退出");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)file, L"文件");

    HMENU edit = CreatePopupMenu();
    AppendMenuW(edit, MF_STRING, IDM_UNDO, L"撤销\tCtrl+Z");
    AppendMenuW(edit, MF_STRING, IDM_REDO, L"重做\tCtrl+Y");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, IDM_FIND, L"查找...\tCtrl+F");
    AppendMenuW(edit, MF_STRING, IDM_REPLACE, L"替换...\tCtrl+H");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)edit, L"编辑");

    HMENU help = CreatePopupMenu();
    AppendMenuW(help, MF_STRING, IDM_ABOUT, L"关于");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)help, L"帮助");
    return m;
}

static LRESULT CALLBACK FrameWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hwndFrame = hwnd; // set before NewDoc() uses it as parent
        g_hwndTab = CreateWindowExW(0, WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | TCS_TABS | TCS_SINGLELINE | TCS_RAGGEDRIGHT | WS_CLIPCHILDREN,
            0, 0, 100, 26, hwnd, nullptr, g_hInst, nullptr);
        NewDoc();
        return 0;
    }
    case WM_SIZE:
        Layout();
        return 0;
    case WM_NOTIFY: {
        NMHDR* nm = (NMHDR*)lp;
        if (nm->hwndFrom == g_hwndTab && nm->code == TCN_SELCHANGE) {
            int sel = TabCtrl_GetCurSel(g_hwndTab);
            if (sel >= 0 && sel < (int)g_docs.size()) SwitchTo(sel);
            return 0;
        }
        SCNotification* sc = (SCNotification*)lp;
        for (size_t i = 0; i < g_docs.size(); i++) {
            if (g_docs[i].sci == nm->hwndFrom) {
                if (sc->nmhdr.code == SCN_SAVEPOINTLEFT) {
                    g_docs[i].dirty = true; UpdateTabText((int)i); UpdateTitle();
                } else if (sc->nmhdr.code == SCN_SAVEPOINTREACHED) {
                    g_docs[i].dirty = false; UpdateTabText((int)i); UpdateTitle();
                }
                break;
            }
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        switch (id) {
        case IDM_NEW:    NewDoc(); break;
        case IDM_OPEN:   OnOpen(); break;
        case IDM_SAVE:   OnSave(); break;
        case IDM_SAVEAS: OnSaveAs(); break;
        case IDM_CLOSE:  if (g_active >= 0) CloseDoc(g_active); break;
        case IDM_EXIT:   SendMessageW(hwnd, WM_CLOSE, 0, 0); break;
        case IDM_UNDO:   if (g_active >= 0) Editor::Undo(g_docs[g_active].sci); break;
        case IDM_REDO:   if (g_active >= 0) Editor::Redo(g_docs[g_active].sci); break;
        case IDM_FIND:   if (g_active >= 0) ShowFindReplace(hwnd, g_docs[g_active].sci, false); break;
        case IDM_REPLACE:if (g_active >= 0) ShowFindReplace(hwnd, g_docs[g_active].sci, true); break;
        case IDM_ABOUT:
            MessageBoxW(hwnd, L"MdPad\n极简 Markdown 编辑器\n纯 Win32 + Scintilla",
                        L"关于 MdPad", MB_OK);
            break;
        }
        return 0;
    }
    case WM_CLOSE: {
        bool any = false;
        for (auto& d : g_docs) if (d.dirty) any = true;
        if (any && MessageBoxW(hwnd, L"有未保存的文档，确定退出？", L"MdPad",
                               MB_YESNO | MB_ICONQUESTION) != IDYES)
            return 0;
        DestroyWindow(hwnd);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    g_hInst = hInst;

    INITCOMMONCONTROLSEX icc; ZeroMemory(&icc, sizeof(icc));
    icc.dwSize = sizeof(icc); icc.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);

    if (!Editor::InitLibraries()) {
        MessageBoxW(nullptr,
            L"无法加载 Scintilla.dll / Lexilla.dll。\n请将这两个 DLL 放在程序同一目录下。",
            L"MdPad", MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW wc; ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = FrameWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = FRAME_CLASS;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    g_hwndFrame = CreateWindowExW(0, FRAME_CLASS, L"MdPad",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 920, 660,
        nullptr, nullptr, hInst, nullptr);
    SetMenu(g_hwndFrame, CreateMainMenu());
    ShowWindow(g_hwndFrame, nCmdShow);
    UpdateWindow(g_hwndFrame);

    ACCEL acc[5];
    memset(acc, 0, sizeof(acc));
    acc[0].fVirt = FCONTROL | FVIRTKEY; acc[0].key = 'N'; acc[0].cmd = IDM_NEW;
    acc[1].fVirt = FCONTROL | FVIRTKEY; acc[1].key = 'O'; acc[1].cmd = IDM_OPEN;
    acc[2].fVirt = FCONTROL | FVIRTKEY; acc[2].key = 'S'; acc[2].cmd = IDM_SAVE;
    acc[3].fVirt = FCONTROL | FVIRTKEY; acc[3].key = 'F'; acc[3].cmd = IDM_FIND;
    acc[4].fVirt = FCONTROL | FVIRTKEY; acc[4].key = 'H'; acc[4].cmd = IDM_REPLACE;
    g_hAccel = CreateAcceleratorTableW(acc, 5);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!TranslateAcceleratorW(g_hwndFrame, g_hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}
