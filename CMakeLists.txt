cmake_minimum_required(VERSION 3.16)
project(mdpad CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GUI application (no console window).
add_executable(mdpad WIN32
    src/main.cpp
    src/editor.cpp
    src/findreplace.cpp
)

target_include_directories(mdpad PRIVATE src/scintilla)
# comctl32: tab control (InitCommonControlsEx/TabCtrl_*).
# comdlg32: GetOpenFileNameW/GetSaveFileNameW (Open/Save As dialogs).
target_link_libraries(mdpad PRIVATE comctl32 comdlg32)

# Statically link the C/C++ runtime so the .exe has no external
# MSVC/MinGW DLL dependency (only Windows system DLLs remain).
if(MINGW)
    target_link_options(mdpad PRIVATE -static -static-libgcc -static-libstdc++)
endif()
