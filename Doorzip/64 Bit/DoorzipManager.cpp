#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <filesystem>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace fs = std::filesystem;

// Control IDs
#define ID_BTN_BROWSE 101
#define ID_BTN_COMPRESS 102
#define ID_BTN_REFRESH 103
#define ID_EDIT_PATH 104
#define ID_LISTVIEW 105

// Modal Control IDs
#define ID_MODAL_EDIT_BYTES 201
#define ID_MODAL_COMBO_EXT  202
#define ID_MODAL_BTN_EXEC   203

HWND g_hListView, g_hEditPath;
std::wstring g_currentPath;
bool g_isUpdatingPath = false;

// --- Core Byte Padding Engine ---
bool targetByteCompress(const std::wstring& inputPath, const std::wstring& outputPath, size_t targetBytes) {
    std::ifstream inFile(inputPath, std::ios::binary | std::ios::ate);
    if (!inFile.is_open()) return false;

    std::streamsize currentSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    if (static_cast<size_t>(currentSize) > targetBytes) return false;

    std::vector<char> buffer(static_cast<size_t>(currentSize));
    if (!inFile.read(buffer.data(), currentSize)) return false;
    inFile.close();

    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile.is_open()) return false;

    outFile.write(buffer.data(), currentSize);

    size_t paddingNeeded = targetBytes - static_cast<size_t>(currentSize);
    std::vector<char> zeroPad(4096, 0);
    while (paddingNeeded > 0) {
        size_t writeSize = (std::min)(paddingNeeded, zeroPad.size());
        outFile.write(zeroPad.data(), writeSize);
        paddingNeeded -= writeSize;
    }

    outFile.close();
    return true;
}

// --- Directory GUI Operations ---
void LoadDirectory(HWND hListView, const std::wstring& path) {
    if (!fs::exists(path) || !fs::is_directory(path)) return;
    
    ListView_DeleteAllItems(hListView);
    g_currentPath = path;

    int index = 0;
    try {
        // Add parent directory navigation item "[..]"
        LVITEMW lvi = { 0 };
        lvi.mask = LVIF_TEXT;
        lvi.iItem = index;
        lvi.pszText = const_cast<wchar_t*>(L"[..]");
        ListView_InsertItem(hListView, &lvi);
        ListView_SetItemText(hListView, index, 1, const_cast<wchar_t*>(L""));
        ListView_SetItemText(hListView, index, 2, const_cast<wchar_t*>(L"Parent Directory"));
        index++;

        for (const auto& entry : fs::directory_iterator(path)) {
            lvi.iItem = index;
            std::wstring filename = entry.path().filename().wstring();
            lvi.pszText = const_cast<wchar_t*>(filename.c_str());
            ListView_InsertItem(hListView, &lvi);

            if (entry.is_directory()) {
                ListView_SetItemText(hListView, index, 1, const_cast<wchar_t*>(L""));
                ListView_SetItemText(hListView, index, 2, const_cast<wchar_t*>(L"Folder"));
            } else {
                std::wstring sizeStr = std::to_wstring(entry.file_size()) + L" bytes";
                ListView_SetItemText(hListView, index, 1, const_cast<wchar_t*>(sizeStr.c_str()));
                ListView_SetItemText(hListView, index, 2, const_cast<wchar_t*>(L"File"));
            }
            index++;
        }
    } catch (...) {}
}

// --- Main Window Event Handler ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Toolbar Buttons matching Python UI layout
        CreateWindowW(L"BUTTON", L"📂 Add/Open", WS_CHILD | WS_VISIBLE, 10, 10, 110, 30, hWnd, (HMENU)ID_BTN_BROWSE, NULL, NULL);
        CreateWindowW(L"BUTTON", L"📦 Target Byte Compress", WS_CHILD | WS_VISIBLE, 125, 10, 180, 30, hWnd, (HMENU)ID_BTN_COMPRESS, NULL, NULL);
        CreateWindowW(L"BUTTON", L"🔄 Refresh", WS_CHILD | WS_VISIBLE, 310, 10, 100, 30, hWnd, (HMENU)ID_BTN_REFRESH, NULL, NULL);

        // Path Bar Frame controls
        CreateWindowW(L"STATIC", L"Path:", WS_CHILD | WS_VISIBLE, 10, 52, 40, 25, hWnd, NULL, NULL, NULL);
        wchar_t userPath[MAX_PATH];
        ExpandEnvironmentStringsW(L"%USERPROFILE%", userPath, MAX_PATH);
        
        g_isUpdatingPath = true;
        g_hEditPath = CreateWindowW(L"EDIT", userPath, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 55, 50, 720, 25, hWnd, (HMENU)ID_EDIT_PATH, NULL, NULL);
        g_isUpdatingPath = false;

        // Tree / ListView Control (Name, Size, Type matching Python columns)
        g_hListView = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
            10, 85, 765, 350, hWnd, (HMENU)ID_LISTVIEW, NULL, NULL);
        ListView_SetExtendedListViewStyle(g_hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;
        lvc.cx = 400; lvc.pszText = const_cast<wchar_t*>(L"Name");
        ListView_InsertColumn(g_hListView, 0, &lvc);
        lvc.cx = 140; lvc.pszText = const_cast<wchar_t*>(L"Size");
        ListView_InsertColumn(g_hListView, 1, &lvc);
        lvc.cx = 205; lvc.pszText = const_cast<wchar_t*>(L"Type");
        ListView_InsertColumn(g_hListView, 2, &lvc);

        LoadDirectory(g_hListView, userPath);
        break;
    }
    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        MoveWindow(g_hListView, 10, 85, width - 20, height - 95, TRUE);
        MoveWindow(g_hEditPath, 55, 50, width - 65, 25, TRUE);
        break;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        if (wmId == ID_BTN_REFRESH) {
            wchar_t pathBuf[MAX_PATH];
            GetWindowTextW(g_hEditPath, pathBuf, MAX_PATH);
            LoadDirectory(g_hListView, pathBuf);
        }
        else if (wmId == ID_BTN_BROWSE) {
            wchar_t folderPath[MAX_PATH] = { 0 };
            BROWSEINFOW bi = { 0 };
            bi.hwndOwner = hWnd;
            bi.lpszTitle = L"Select Directory to Open";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl != NULL) {
                if (SHGetPathFromIDListW(pidl, folderPath)) {
                    g_isUpdatingPath = true;
                    SetWindowTextW(g_hEditPath, folderPath);
                    g_isUpdatingPath = false;
                    LoadDirectory(g_hListView, folderPath);
                }
                CoTaskMemFree(pidl);
            }
        }
        else if (wmId == ID_EDIT_PATH && wmEvent == EN_CHANGE) {
            if (!g_isUpdatingPath) {
                wchar_t pathBuf[MAX_PATH];
                GetWindowTextW(g_hEditPath, pathBuf, MAX_PATH);
                if (fs::exists(pathBuf) && fs::is_directory(pathBuf)) {
                    LoadDirectory(g_hListView, pathBuf);
                }
            }
        }
        else if (wmId == ID_BTN_COMPRESS) {
            int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (sel == -1) {
                MessageBoxW(hWnd, L"Select a file or folder first!", L"Warning", MB_OK | MB_ICONWARNING);
                break;
            }
            wchar_t itemName[MAX_PATH];
            ListView_GetItemText(g_hListView, sel, 0, itemName, MAX_PATH);

            if (wcscmp(itemName, L"[..]") == 0) break;

            std::wstring targetPath = g_currentPath + L"\\" + itemName;

            // Create Modal Dialog Window dynamically on the fly
            HWND hModal = CreateWindowExW(WS_EX_DLGMODALFRAME, (LPCWSTR)32770, L"ZDI - Byte Target Compression",
                WS_VISIBLE | WS_CAPTION | WS_SYSMENU, 
                (GetSystemMetrics(SM_CXSCREEN) - 360) / 2, (GetSystemMetrics(SM_CYSCREEN) - 220) / 2, 
                360, 220, hWnd, NULL, NULL, NULL);

            CreateWindowW(L"STATIC", (L"Target: " + std::wstring(itemName)).c_str(), WS_CHILD | WS_VISIBLE, 20, 15, 300, 20, hModal, NULL, NULL, NULL);
            CreateWindowW(L"STATIC", L"Select Container Extension:", WS_CHILD | WS_VISIBLE, 20, 42, 200, 20, hModal, NULL, NULL, NULL);
            HWND hCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 20, 65, 300, 120, hModal, (HMENU)ID_MODAL_COMBO_EXT, NULL, NULL);
            
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L".dzi");
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L".Zd");
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L".dzz");
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L".DoorZ");
            SendMessageW(hCombo, CB_SETCURSEL, 0, 0);

            CreateWindowW(L"STATIC", L"Enter Exact Target Size (Bytes):", WS_CHILD | WS_VISIBLE, 20, 100, 250, 20, hModal, NULL, NULL, NULL);
            HWND hEditBytes = CreateWindowW(L"EDIT", L"1000000", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 20, 122, 300, 25, hModal, (HMENU)ID_MODAL_EDIT_BYTES, NULL, NULL);
            
            HWND hBtnExec = CreateWindowW(L"BUTTON", L"Compress Now", WS_CHILD | WS_VISIBLE, 110, 155, 130, 30, hModal, (HMENU)ID_MODAL_BTN_EXEC, NULL, NULL);

            MSG modalMsg;
            EnableWindow(hWnd, FALSE);
            while (IsWindow(hModal) && GetMessageW(&modalMsg, NULL, 0, 0)) {
                if (modalMsg.message == WM_COMMAND && LOWORD(modalMsg.wParam) == ID_MODAL_BTN_EXEC) {
                    wchar_t bytesText[32];
                    GetWindowTextW(hEditBytes, bytesText, 32);
                    size_t targetSize = _wtoi64(bytesText);

                    wchar_t extText[16];
                    int selIdx = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
                    SendMessageW(hCombo, CB_GETLBTEXT, selIdx, (LPARAM)extText);

                    if (targetSize > 0) {
                        std::wstring outputPath = targetPath + extText;
                        if (targetByteCompress(targetPath, outputPath, targetSize)) {
                            MessageBoxW(hModal, (L"Saved as:\n" + outputPath).c_str(), L"Success", MB_OK);
                            DestroyWindow(hModal);
                            LoadDirectory(g_hListView, g_currentPath);
                        } else {
                            MessageBoxW(hModal, L"Error: Target size smaller than file payload.", L"Error", MB_OK | MB_ICONERROR);
                        }
                    }
                }
                TranslateMessage(&modalMsg);
                DispatchMessageW(&modalMsg);
            }
            EnableWindow(hWnd, TRUE);
            SetFocus(hWnd);
        }
        break;
    }
    case WM_NOTIFY: {
        LPNMHDR lpnmHeader = (LPNMHDR)lParam;
        if (lpnmHeader->idFrom == ID_LISTVIEW && lpnmHeader->code == NM_DBLCLK) {
            LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lParam;
            if (lpnmitem->iItem != -1) {
                wchar_t itemName[MAX_PATH];
                ListView_GetItemText(g_hListView, lpnmitem->iItem, 0, itemName, MAX_PATH);

                if (wcscmp(itemName, L"[..]") == 0) {
                    fs::path parent = fs::path(g_currentPath).parent_path();
                    g_isUpdatingPath = true;
                    SetWindowTextW(g_hEditPath, parent.c_str());
                    g_isUpdatingPath = false;
                    LoadDirectory(g_hListView, parent.wstring());
                } else {
                    std::wstring fullPath = g_currentPath + L"\\" + itemName;
                    if (fs::is_directory(fullPath)) {
                        g_isUpdatingPath = true;
                        SetWindowTextW(g_hEditPath, fullPath.c_str());
                        g_isUpdatingPath = false;
                        LoadDirectory(g_hListView, fullPath);
                    }
                }
            }
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ZDIFileManagerClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);
    HWND hWnd = CreateWindowW(L"ZDIFileManagerClass", L"ZDI - Flagship File Manager (C++ Native)", 
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 810, 520, NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}