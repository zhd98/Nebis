#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

#include "editor.h"
#include "Scintilla.h"
#include "ILexer.h"
#include "Lexilla.h"
#include "SciLexer.h"
#include <string>

static HMODULE g_hSci = nullptr;
static HMODULE g_hLex = nullptr;

bool Editor::InitLibraries() {
    g_hSci = LoadLibraryA("Scintilla.dll");
    if (!g_hSci) return false;
    g_hLex = LoadLibraryA("Lexilla.dll");
    if (!g_hLex) {
        FreeLibrary(g_hSci);
        g_hSci = nullptr;
        return false;
    }
    return true;
}

// Attach the markdown lexer (from Lexilla.dll) to a Scintilla instance.
static void AttachMarkdownLexer(HWND sci) {
    if (!g_hLex) return;
    auto pCreate = reinterpret_cast<Lexilla::CreateLexerFn>(
        GetProcAddress(g_hLex, LEXILLA_CREATELEXER));
    if (!pCreate) return;
    Scintilla::ILexer5* lexer = pCreate("markdown");
    if (!lexer) return;
    SendMessageW(sci, SCI_SETILEXER, 0, reinterpret_cast<LPARAM>(lexer));
}

void Editor::SetupMarkdown(HWND sci) {
    // Base style: proportional, comfortable size, near-black text.
    SendMessageW(sci, SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<LPARAM>("Consolas"));
    SendMessageW(sci, SCI_STYLESETSIZE, STYLE_DEFAULT, 11);
    SendMessageW(sci, SCI_STYLESETFORE, STYLE_DEFAULT, RGB(0x20, 0x20, 0x20));
    SendMessageW(sci, SCI_STYLECLEARALL, 0, 0);

    // Structural markdown symbols (#, >, -, *, list/quote markers) -> dim gray,
    // so they visually recede while the content stands out.
    COLORREF dim = RGB(0xA6, 0xA6, 0xA6);
    SendMessageW(sci, SCI_STYLESETFORE, SCE_MARKDOWN_LINE_BEGIN, dim);
    SendMessageW(sci, SCI_STYLESETSIZE, SCE_MARKDOWN_LINE_BEGIN, 10);
    SendMessageW(sci, SCI_STYLESETFORE, SCE_MARKDOWN_ULIST_ITEM, dim);
    SendMessageW(sci, SCI_STYLESETFORE, SCE_MARKDOWN_OLIST_ITEM, dim);
    SendMessageW(sci, SCI_STYLESETFORE, SCE_MARKDOWN_BLOCKQUOTE, dim);
    SendMessageW(sci, SCI_STYLESETFORE, SCE_MARKDOWN_PRECHAR, dim);

    // Headings: progressively smaller, bold and colored (this is the only
    // "size/color distinction" we need - no preview pane).
    struct Hd { int style; int size; COLORREF c; };
    Hd heads[] = {
        { SCE_MARKDOWN_HEADER1, 20, RGB(0xC0, 0x39, 0x2B) },
        { SCE_MARKDOWN_HEADER2, 17, RGB(0xD3, 0x54, 0x00) },
        { SCE_MARKDOWN_HEADER3, 15, RGB(0x8E, 0x44, 0xAD) },
        { SCE_MARKDOWN_HEADER4, 13, RGB(0x16, 0xA0, 0x85) },
        { SCE_MARKDOWN_HEADER5, 12, RGB(0x2C, 0x3E, 0x50) },
        { SCE_MARKDOWN_HEADER6, 11, RGB(0x7F, 0x8C, 0x8D) },
    };
    for (auto& h : heads) {
        SendMessageW(sci, SCI_STYLESETSIZE, h.style, h.size);
        SendMessageW(sci, SCI_STYLESETFORE, h.style, h.c);
        SendMessageW(sci, SCI_STYLESETBOLD, h.style, 1);
    }

    // Inline code (monospace green) and links (blue).
    SendMessageW(sci, SCI_STYLESETFORE, SCE_MARKDOWN_CODE, RGB(0x0B, 0x6E, 0x0B));
    SendMessageW(sci, SCI_STYLESETFONT, SCE_MARKDOWN_CODE, reinterpret_cast<LPARAM>("Consolas"));
    SendMessageW(sci, SCI_STYLESETFORE, SCE_MARKDOWN_CODE2, RGB(0x0B, 0x6E, 0x0B));
    SendMessageW(sci, SCI_STYLESETFONT, SCE_MARKDOWN_CODE2, reinterpret_cast<LPARAM>("Consolas"));
    SendMessageW(sci, SCI_STYLESETFORE, SCE_MARKDOWN_LINK, RGB(0x1A, 0x0D, 0xAB));

    // Emphasis / strong.
    SendMessageW(sci, SCI_STYLESETITALIC, SCE_MARKDOWN_EM1, 1);
    SendMessageW(sci, SCI_STYLESETITALIC, SCE_MARKDOWN_EM2, 1);
    SendMessageW(sci, SCI_STYLESETBOLD, SCE_MARKDOWN_STRONG1, 1);
    SendMessageW(sci, SCI_STYLESETBOLD, SCE_MARKDOWN_STRONG2, 1);

    AttachMarkdownLexer(sci);
    SendMessageW(sci, SCI_COLOURISE, 0, -1);
}

HWND Editor::Create(HWND parent, int x, int y, int w, int h) {
    HWND sci = CreateWindowExW(
        0, L"Scintilla", L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | WS_TABSTOP,
        x, y, w, h, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (sci) {
        SendMessageW(sci, SCI_SETCODEPAGE, CP_UTF8, 0);
        SendMessageW(sci, SCI_SETMARGINWIDTHN, 0, 0);   // hide line-number gutter (Notepad-like)
        SendMessageW(sci, SCI_SETWRAPMODE, SC_WRAP_WORD, 0);
        SendMessageW(sci, SCI_SETTABWIDTH, 4, 0);
        SendMessageW(sci, SCI_SETEOLMODE, SC_EOL_CRLF, 0);
        SetupMarkdown(sci);
    }
    return sci;
}

bool Editor::LoadFile(HWND sci, const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD size = GetFileSize(h, nullptr);
    std::string data;
    data.resize(size + 1);
    DWORD read = 0;
    bool ok = ReadFile(h, data.data(), size, &read, nullptr);
    CloseHandle(h);
    if (!ok) return false;
    data[read] = '\0';

    const char* p = data.data();
    if (read >= 3 &&
        (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
        p += 3; // strip UTF-8 BOM
    }

    SendMessageW(sci, SCI_SETCODEPAGE, CP_UTF8, 0);
    SendMessageW(sci, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(p));
    SendMessageW(sci, SCI_EMPTYUNDOBUFFER, 0, 0);
    SendMessageW(sci, SCI_SETSAVEPOINT, 0, 0);
    return true;
}

bool Editor::SaveFile(HWND sci, const std::wstring& path) {
    long len = SendMessageW(sci, SCI_GETLENGTH, 0, 0);
    std::string buf;
    buf.resize(len + 1);
    SendMessageW(sci, SCI_GETTEXT, len + 1, reinterpret_cast<LPARAM>(buf.data()));

    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(h, buf.data(), len, &written, nullptr);
    CloseHandle(h);
    if (ok) SendMessageW(sci, SCI_SETSAVEPOINT, 0, 0);
    return ok;
}

void Editor::Undo(HWND sci)      { SendMessageW(sci, SCI_UNDO, 0, 0); }
void Editor::Redo(HWND sci)      { SendMessageW(sci, SCI_REDO, 0, 0); }
bool Editor::CanUndo(HWND sci)   { return SendMessageW(sci, SCI_CANUNDO, 0, 0) != 0; }
bool Editor::CanRedo(HWND sci)   { return SendMessageW(sci, SCI_CANREDO, 0, 0) != 0; }
void Editor::GrabFocus(HWND sci) { SendMessageW(sci, SCI_GRABFOCUS, 0, 0); }
long Editor::GetLength(HWND sci) { return SendMessageW(sci, SCI_GETLENGTH, 0, 0); }
