#pragma once
#include <windows.h>

// Show the Find (or Replace) window bound to the given Scintilla editor.
void ShowFindReplace(HWND owner, HWND sci, bool replace);
