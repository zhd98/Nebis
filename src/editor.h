#pragma once
#include <windows.h>
#include <string>
#include <vector>

// One open document: a Scintilla child window + its path/dirty state.
struct Document {
    HWND sci = nullptr;     // Scintilla editor window
    std::wstring path;      // empty => untitled
    int tab = 0;            // index in the tab control
    bool dirty = false;
};

namespace Editor {

// Load Scintilla.dll and Lexilla.dll. Must be called once at startup.
// Returns false if either DLL cannot be found (they must sit next to the exe).
bool InitLibraries();

// Create a Scintilla child editor window.
HWND Create(HWND parent, int x, int y, int w, int h);

// Apply the markdown lexer + styles (gray structural markers, big colored headings).
void SetupMarkdown(HWND sci);

// UTF-8 aware file load/save. Returns false on error.
bool LoadFile(HWND sci, const std::wstring& path);
bool SaveFile(HWND sci, const std::wstring& path);

// Small wrappers around Scintilla messages.
void Undo(HWND sci);
void Redo(HWND sci);
bool CanUndo(HWND sci);
bool CanRedo(HWND sci);
void GrabFocus(HWND sci);
long GetLength(HWND sci);

} // namespace Editor

// Show the Find (or Replace) window bound to the given Scintilla editor.
void ShowFindReplace(HWND owner, HWND sci, bool replace);
