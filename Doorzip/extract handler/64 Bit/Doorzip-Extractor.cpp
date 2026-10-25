#include <windows.h>
#include <commdlg.h>
#include <string>

// Control IDs
#define ID_EDIT_PATH    101
#define ID_BTN_BROWSE   102
#define ID_BTN_EXTRACT  103

HWND hEditPath;

// Forward declaration of core extraction function (replace or link with your dzi_core logic)
bool ExtractDzi(const std::wstring& archivePath, const std::wstring& extractDir) {
    // TODO: Integrate your C++ archive extraction logic here.
    // Return true on success, false on failure.
    return true; 
}

// Function to handle the Browse button file dialog
void OpenBrowseDialog(HWND hwnd) {
    OPENFILENAMEW ofn;
    wchar_t szFile[260] = { 0 };

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Doorzip Archives (*.dzi;*.zd;*.dzz)\0*.dzi;*.zd;*.dzz\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn) == TRUE) {
        SetWindowTextW(hEditPath, ofn.lpstrFile);
    }
}

// Function to handle extraction execution
void ExecuteExtraction(HWND hwnd) {
    wchar_t szPath[260];
    GetWindowTextW(hEditPath, szPath, 260);
    std::wstring archivePath(szPath);

    if (archivePath.empty()) {
        MessageBoxW(hwnd, L"Please select a valid archive file first!", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Default extraction directory is right next to the file, named without extension
    size_t lastDot = archivePath.find_last_of(L".");
    std::wstring extractDir = (lastDot == std::wstring::npos) ? archivePath : archivePath.substr(0, lastDot);

    // Call extraction core logic
    bool success = ExtractDzi(archivePath, extractDir);

    if (success) {
        std::wstring msg = L"Successfully extracted!\n\nFolder: " + extractDir;
        MessageBoxW(hwnd, msg.c_str(), L"Success", MB_OK | MB_ICONINFORMATION);
        SetWindowTextW(hEditPath, L"");
    } else {
        MessageBoxW(hwnd, L"Extraction failed. Invalid container or padding error.", L"Error", MB_OK | MB_ICONERROR);
    }
}

// Window Procedure to handle UI events
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        // Create UI elements matching the layout
        CreateWindowW(L"STATIC", L"Select a .DZi, .ZD, or .DZZ archive to extract:",
            WS_VISIBLE | WS_CHILD, 20, 20, 450, 20, hwnd, NULL, NULL, NULL);

        hEditPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 20, 50, 420, 25, hwnd, (HMENU)ID_EDIT_PATH, NULL, NULL);

        CreateWindowW(L"BUTTON", L"📂 Browse...",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 450, 50, 110, 25, hwnd, (HMENU)ID_BTN_BROWSE, NULL, NULL);

        CreateWindowW(L"BUTTON", L"📦 Extract Archive Now",
            WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 150, 120, 280, 40, hwnd, (HMENU)ID_BTN_EXTRACT, NULL, NULL);
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BTN_BROWSE) {
            OpenBrowseDialog(hwnd);
        }
        else if (LOWORD(wParam) == ID_BTN_EXTRACT) {
            ExecuteExtraction(hwnd);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// Entry Point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"DoorzipExtractorClass";

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Doorzip - Dedicated Extractor",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 230,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}