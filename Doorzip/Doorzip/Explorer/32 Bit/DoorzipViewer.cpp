#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <filesystem>
namespace fs = std::filesystem;
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

struct ArchiveEntry {
    std::wstring name;
    uint64_t size;
};

std::vector<ArchiveEntry> g_archiveEntries;
bool g_inArchive = false;
std::wstring g_currentArchive;

HWND g_hListView, g_hStatus, g_hEditPath;
wchar_t g_currentPath[MAX_PATH] = L"C:\\";
WNDPROC g_oldEditProc = nullptr;

bool PickFolderModern(HWND hWnd, const wchar_t* title, wchar_t* outPath) {
    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pfd));
    if (FAILED(hr) || !pfd) return false;
    DWORD opts = 0;
    pfd->GetOptions(&opts);
    pfd->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    pfd->SetTitle(title);
    hr = pfd->Show(hWnd);
    if (SUCCEEDED(hr)) {
        IShellItem* psi = nullptr;
        hr = pfd->GetResult(&psi);
        if (SUCCEEDED(hr) && psi) {
            PWSTR pszPath = nullptr;
            hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
            if (SUCCEEDED(hr) && pszPath) {
                wcscpy_s(outPath, MAX_PATH, pszPath);
                CoTaskMemFree(pszPath);
                psi->Release();
                pfd->Release();
                return true;
            }
            if (psi) psi->Release();
        }
    }
    pfd->Release();
    return false;
}

std::vector<unsigned char> DecompressRAD(const std::vector<unsigned char>& compressed) {
    std::vector<unsigned char> out;
    if (compressed.size() < 3 || memcmp(compressed.data(), "RAD", 3) != 0)
        return out;

    size_t i = 3;
    std::vector<char> chunk(1024 * 1024, 0);
    while (i < compressed.size()) {
        if (i + 4 < compressed.size() && compressed[i] == 'R' && compressed[i + 1] == 'A' && compressed[i + 3] == 'D') {
            unsigned char cur = compressed[i + 2];
            i += 4;
            uint32_t run = 0;
            if (i + 4 <= compressed.size()) {
                memcpy(&run, &compressed[i], 4);
                i += 4;
            } else break;
            while (run > 0) {
                size_t toWrite = (run > chunk.size()) ? chunk.size() : run;
                memset(chunk.data(), cur, toWrite);
                out.insert(out.end(), chunk.begin(), chunk.begin() + toWrite);
                run -= (uint32_t)toWrite;
            }
        } else if (i + 1 < compressed.size() && compressed[i] == 'R' && compressed[i + 1] == 'R') {
            out.push_back('R');
            i += 2;
        } else {
            out.push_back(compressed[i]);
            i++;
        }
    }
    return out;
}

bool ParseBlob(const std::vector<unsigned char>& data) {
    g_archiveEntries.clear();
    size_t pos = 0;
    while (pos + 4 <= data.size()) {
        uint32_t nl = 0;
        memcpy(&nl, &data[pos], 4);
        pos += 4;
        if (nl == 0 || nl > 1024 || pos + nl > data.size()) break;

        std::string rel(nl, ' ');
        memcpy(rel.data(), &data[pos], nl);
        pos += nl;

        if (pos + 8 > data.size()) break;
        uint64_t fsz = 0;
        memcpy(&fsz, &data[pos], 8);
        pos += 8;

        if (pos + fsz > data.size()) break;

        std::wstring wname(rel.begin(), rel.end());
        g_archiveEntries.push_back({ wname, fsz });
        pos += (size_t)fsz;
    }
    return !g_archiveEntries.empty();
}

bool LoadArchive(const std::wstring& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::vector<unsigned char> raw((std::istreambuf_iterator<char>(in)), {});
    in.close();

    auto decompressed = DecompressRAD(raw);
    const std::vector<unsigned char>& blob = decompressed.empty() ? raw : decompressed;

    if (ParseBlob(blob)) {
        g_currentArchive = path;
        g_inArchive = true;
        return true;
    }
    return false;
}

void ShowArchiveContents() {
    ListView_DeleteAllItems(g_hListView);
    LVITEMW it = { 0 };
    it.mask = LVIF_TEXT;
    it.iItem = 0;
    it.pszText = (LPWSTR)L"[..]";
    ListView_InsertItem(g_hListView, &it);

    for (size_t i = 0; i < g_archiveEntries.size(); ++i) {
        it.iItem = (int)i + 1;
        it.pszText = (LPWSTR)g_archiveEntries[i].name.c_str();
        ListView_InsertItem(g_hListView, &it);

        wchar_t sz[64];
        if (g_archiveEntries[i].size < 1024)
            wsprintfW(sz, L"%llu B", g_archiveEntries[i].size);
        else if (g_archiveEntries[i].size < 1024ULL * 1024)
            wsprintfW(sz, L"%.1f KB", g_archiveEntries[i].size / 1024.0);
        else
            wsprintfW(sz, L"%.1f MB", g_archiveEntries[i].size / (1024.0 * 1024.0));
        ListView_SetItemText(g_hListView, (int)i + 1, 1, sz);
    }
    SetWindowTextW(g_hStatus, L"Viewing archive contents");
}

void RefreshFileList() {
    if (g_inArchive) {
        ShowArchiveContents();
        return;
    }
    ListView_DeleteAllItems(g_hListView);
    WIN32_FIND_DATAW fd;
    wchar_t search[MAX_PATH];
    wsprintfW(search, L"%s\\*", g_currentPath);
    HANDLE h = FindFirstFileW(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    int idx = 0;
    LVITEMW it = { 0 };
    it.mask = LVIF_TEXT;
    it.iItem = idx;
    it.pszText = (LPWSTR)L"[..]";
    ListView_InsertItem(g_hListView, &it);
    idx++;

    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        it.iItem = idx;
        it.pszText = fd.cFileName;
        ListView_InsertItem(g_hListView, &it);

        bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isDir) {
            ListView_SetItemText(g_hListView, idx, 1, (LPWSTR)L"<DIR>");
        } else {
            wchar_t s[64];
            wsprintfW(s, L"%lu KB", fd.nFileSizeLow / 1024);
            ListView_SetItemText(g_hListView, idx, 1, s);
        }
        idx++;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    SetWindowTextW(g_hStatus, L"Doorzip Viewer - Select .dzi, .dzz, .zd, .doorz to view");
}

void NavigateToTypedPath() {
    if (g_inArchive) return;
    wchar_t typed[MAX_PATH];
    GetWindowTextW(g_hEditPath, typed, MAX_PATH);
    PathRemoveBlanksW(typed);
    size_t tlen = wcslen(typed);
    if (tlen > 3 && typed[tlen - 1] == L'\\') typed[tlen - 1] = 0;
    if (!PathFileExistsW(typed)) {
        MessageBoxW(nullptr, typed, L"Folder not found", MB_OK);
        return;
    }
    wcscpy_s(g_currentPath, MAX_PATH, typed);
    SetWindowTextW(g_hEditPath, g_currentPath);
    RefreshFileList();
}

LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        NavigateToTypedPath();
        return 0;
    }
    return CallWindowProcW(g_oldEditProc, hWnd, msg, wParam, lParam);
}

#define ID_LISTVIEW 1001
#define ID_EDIT_PATH 1005
#define ID_BTN_BROWSE 1006
#define ID_BTN_VIEW 1007

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        InitCommonControls();
        g_hListView = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_SINGLESEL | WS_BORDER,
            10, 10, 760, 370, hWnd, (HMENU)ID_LISTVIEW, GetModuleHandleW(nullptr), nullptr);
        ListView_SetExtendedListViewStyle(g_hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        LVCOLUMNW col = { 0 };
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = 600;
        col.pszText = (LPWSTR)L"Name";
        ListView_InsertColumn(g_hListView, 0, &col);
        col.cx = 150;
        col.pszText = (LPWSTR)L"Size";
        ListView_InsertColumn(g_hListView, 1, &col);

        g_hEditPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_currentPath, WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
            10, 395, 450, 28, hWnd, (HMENU)ID_EDIT_PATH, GetModuleHandleW(nullptr), nullptr);
        g_oldEditProc = (WNDPROC)SetWindowLongPtrW(g_hEditPath, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

        CreateWindowW(L"BUTTON", L"Browse...", WS_VISIBLE | WS_CHILD, 470, 394, 90, 30, hWnd, (HMENU)ID_BTN_BROWSE, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"View", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 570, 394, 190, 30, hWnd, (HMENU)ID_BTN_VIEW, nullptr, nullptr);
        g_hStatus = CreateWindowW(L"STATIC", L"Doorzip Viewer - Select .dzi, .dzz, .zd, .doorz to view", WS_VISIBLE | WS_CHILD,
            10, 435, 760, 20, hWnd, nullptr, nullptr, nullptr);
        RefreshFileList();
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == ID_BTN_BROWSE) {
            wchar_t picked[MAX_PATH] = L"";
            if (PickFolderModern(hWnd, L"Select folder", picked)) {
                g_inArchive = false;
                wcscpy_s(g_currentPath, MAX_PATH, picked);
                SetWindowTextW(g_hEditPath, g_currentPath);
                RefreshFileList();
            }
        }
        if (id == ID_BTN_VIEW) {
            if (g_inArchive) {
                MessageBoxW(hWnd, L"Already viewing an archive. Double-click [..] to go back.", L"Doorzip Viewer", MB_OK);
                break;
            }
            int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (sel <= 0) {
                MessageBoxW(hWnd, L"Select an archive file first!", L"Doorzip Viewer", MB_OK);
                break;
            }
            wchar_t n[MAX_PATH];
            ListView_GetItemText(g_hListView, sel, 0, n, MAX_PATH);
            wchar_t fullPath[MAX_PATH];
            wsprintfW(fullPath, L"%s\\%s", g_currentPath, n);
            if (LoadArchive(fullPath)) {
                ShowArchiveContents();
            } else {
                MessageBoxW(hWnd, L"Could not read archive contents.", L"Doorzip Viewer", MB_OK);
            }
        }
        break;
    }
    case WM_NOTIFY: {
        LPNMHDR hdr = (LPNMHDR)lParam;
        if (hdr->idFrom == ID_LISTVIEW && hdr->code == NM_DBLCLK) {
            int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (sel >= 0) {
                wchar_t n[MAX_PATH];
                ListView_GetItemText(g_hListView, sel, 0, n, MAX_PATH);
                if (!wcscmp(n, L"[..]")) {
                    if (g_inArchive) {
                        g_inArchive = false;
                        g_archiveEntries.clear();
                        RefreshFileList();
                    } else {
                        PathRemoveFileSpecW(g_currentPath);
                        if (!wcslen(g_currentPath)) wcscpy_s(g_currentPath, MAX_PATH, L"C:\\");
                        SetWindowTextW(g_hEditPath, g_currentPath);
                        RefreshFileList();
                    }
                } else if (!g_inArchive) {
                    wchar_t np[MAX_PATH];
                    wsprintfW(np, L"%s\\%s", g_currentPath, n);
                    if (PathIsDirectoryW(np)) {
                        wcscpy_s(g_currentPath, MAX_PATH, np);
                        SetWindowTextW(g_hEditPath, g_currentPath);
                        RefreshFileList();
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

int WINAPI wWinMain(HINSTANCE hI, HINSTANCE, LPWSTR, int nCmd) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hI;
    wc.lpszClassName = L"DoorzipViewer64Class";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    HWND w = CreateWindowW(wc.lpszClassName, L"Doorzip Viewer 32-bit", WS_OVERLAPPEDWINDOW,
        0, 0, 800, 500, nullptr, nullptr, hI, nullptr);
    ShowWindow(w, nCmd);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    CoUninitialize();
    return 0;
}