#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <filesystem>
namespace fs = std::filesystem;
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

bool PickFolderModern(HWND hWnd, const wchar_t* title, wchar_t* outPath){
    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pfd));
    if(FAILED(hr) || !pfd) return false;
    DWORD opts = 0;
    pfd->GetOptions(&opts);
    pfd->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    pfd->SetTitle(title);
    hr = pfd->Show(hWnd);
    if(SUCCEEDED(hr)){
        IShellItem* psi = nullptr;
        hr = pfd->GetResult(&psi);
        if(SUCCEEDED(hr) && psi){
            PWSTR pszPath = nullptr;
            hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
            if(SUCCEEDED(hr) && pszPath){
                wcscpy_s(outPath, MAX_PATH, pszPath);
                CoTaskMemFree(pszPath);
                psi->Release();
                pfd->Release();
                return true;
            }
            if(psi) psi->Release();
        }
    }
    pfd->Release();
    return false;
}

bool DecompressRAD(const std::string& inPath, const std::string& outPath){
    std::ifstream in(inPath, std::ios::binary); if(!in) return false;
    char magic[3]; in.read(magic,3);
    if(in.gcount()!=3 || memcmp(magic,"RAD",3)!=0) return false;
    std::vector<unsigned char> compressed((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    std::ofstream out(outPath, std::ios::binary); if(!out) return false;
    std::vector<char> chunk(1024*1024, 0);
    size_t i=0;
    while(i<compressed.size()){
        if(i+4<compressed.size() && compressed[i]=='R' && compressed[i+1]=='A' && compressed[i+3]=='D'){
            unsigned char cur=compressed[i+2]; i+=4;
            uint32_t run=0;
            if(i+4<=compressed.size()){ memcpy(&run,&compressed[i],4); i+=4; } else break;
            memset(chunk.data(), cur, chunk.size());
            uint32_t left = run;
            while(left > 0){
                size_t toWrite = left > chunk.size() ? chunk.size() : left;
                out.write(chunk.data(), toWrite);
                left -= (uint32_t)toWrite;
            }
        } else if(i+1<compressed.size() && compressed[i]=='R' && compressed[i+1]=='R'){
            out.put('R'); i+=2;
        } else {
            out.put(compressed[i]); i++;
        }
    }
    out.close(); return true;
}

bool UnpackBlob(const std::string& blobPath, const std::wstring& destDir){
    std::ifstream in(blobPath, std::ios::binary); if(!in) return false;
    in.seekg(0, std::ios::end); size_t totalSz = (size_t)in.tellg(); in.seekg(0, std::ios::beg);
    bool unpackedAny=false;
    std::vector<char> buf(1024*1024);
    while(true){
        uint32_t nl=0;
        in.read((char*)&nl,4); if(in.gcount()!=4) break;
        if(nl==0 || nl>1024) break;
        std::string rel(nl,' '); in.read(rel.data(), nl); if((size_t)in.gcount()!=nl) break;
        uint64_t fsz=0; in.read((char*)&fsz,8); if(in.gcount()!=8) break;
        fs::path targetPath=fs::path(destDir)/fs::path(rel);
        fs::create_directories(targetPath.parent_path());
        std::ofstream out(targetPath, std::ios::binary); if(!out) break;
        uint64_t left = fsz;
        while(left > 0){
            size_t toRead = left > buf.size() ? buf.size() : (size_t)left;
            in.read(buf.data(), toRead); if((size_t)in.gcount()!=toRead) break;
            out.write(buf.data(), toRead);
            left -= toRead;
        }
        out.close(); unpackedAny=true;
        if(in.eof()) break;
    }
    if(!unpackedAny && totalSz>0){
        in.clear(); in.seekg(0, std::ios::beg);
        fs::path targetPath=fs::path(destDir)/L"extracted_file.bin";
        fs::create_directories(destDir);
        std::ofstream out(targetPath, std::ios::binary);
        uint64_t left = totalSz;
        while(left > 0){
            size_t toRead = left > buf.size() ? buf.size() : (size_t)left;
            in.read(buf.data(), toRead); size_t got = (size_t)in.gcount(); if(got==0) break;
            out.write(buf.data(), got); left -= got;
        }
        out.close(); unpackedAny = true;
    }
    return unpackedAny;
}

bool Find7zExe(std::wstring& outPath){
    const wchar_t* paths[] = { L"C:\\Program Files\\7-Zip\\7z.exe", L"C:\\Program Files (x86)\\7-Zip\\7z.exe" };
    for(auto p: paths){ if(PathFileExistsW(p)){ outPath=p; return true; } }
    return false;
}

bool Extract7zArchive(const std::wstring& archivePath, const std::wstring& destDir){
    std::wstring sevenZip; if(!Find7zExe(sevenZip)) return false;
    std::wstring cmd = L"\"" + sevenZip + L"\" x -y \"" + archivePath + L"\" -o\"" + destDir + L"\"";
    STARTUPINFOW si={0}; si.cb=sizeof(si); PROCESS_INFORMATION pi={0};
    si.dwFlags=STARTF_USESHOWWINDOW; si.wShowWindow=SW_HIDE;
    std::wstring cmdCopy=cmd;
    if(CreateProcessW(NULL, cmdCopy.data(), NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)){
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode=0; GetExitCodeProcess(pi.hProcess,&exitCode);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        return exitCode==0;
    }
    return false;
}

bool ExtractArchive(const std::wstring& archivePath, const std::wstring& destDir){
    // FIX: Make a folder named after the archive, put everything inside it
    // Doorzip Test Bomb.dzi -> destDir / Doorzip Test Bomb / Leaf_1 ...
    std::wstring stem = fs::path(archivePath).stem().wstring();
    std::wstring finalDest = (fs::path(destDir) / fs::path(stem)).wstring();
    fs::create_directories(finalDest);
    {
        std::ifstream in(archivePath, std::ios::binary);
        if(in){
            char magic[6]={0}; in.read(magic,6); in.close();
            if(memcmp(magic, "7z\xBC\xAF\x27\x1C", 6)==0){
                return Extract7zArchive(archivePath, finalDest);
            }
        }
    }
    char archiveA[MAX_PATH]; WideCharToMultiByte(CP_UTF8,0,archivePath.c_str(),-1,archiveA,MAX_PATH,nullptr,nullptr);
    wchar_t tempPathW[MAX_PATH]; GetTempPathW(MAX_PATH,tempPathW);
    wchar_t tempW[MAX_PATH]; wsprintfW(tempW,L"%sDoorzipUnpack_%lu.bin",tempPathW,GetTickCount());
    char tempA[MAX_PATH]; WideCharToMultiByte(CP_UTF8,0,tempW,-1,tempA,MAX_PATH,nullptr,nullptr);
    if(DecompressRAD(archiveA, tempA)){
        bool res=UnpackBlob(tempA, finalDest);
        fs::remove(tempA);
        return res;
    }
    if(Extract7zArchive(archivePath, finalDest)) return true;
    return false;
}

#define ID_LISTVIEW 1001
#define ID_EDIT_PATH 1005
#define ID_BTN_BROWSE 1006
#define ID_BTN_EXTRACT 1007

HWND g_hListView,g_hStatus,g_hEditPath;
wchar_t g_currentPath[MAX_PATH]=L"C:\\";
WNDPROC g_oldEditProc=nullptr;

void RefreshFileList(){
    ListView_DeleteAllItems(g_hListView);
    WIN32_FIND_DATAW fd; wchar_t search[MAX_PATH]; wsprintfW(search,L"%s\\*",g_currentPath);
    HANDLE h=FindFirstFileW(search,&fd); if(h==INVALID_HANDLE_VALUE) return;
    int idx=0; LVITEMW it={0}; it.mask=LVIF_TEXT; it.iItem=idx; it.pszText=(LPWSTR)L"[..]"; ListView_InsertItem(g_hListView,&it); idx++;
    do{
        if(!wcscmp(fd.cFileName,L".")) continue;
        if(!wcscmp(fd.cFileName,L"..")) continue;
        it.iItem=idx; it.pszText=fd.cFileName; ListView_InsertItem(g_hListView,&it);
        bool isDir=(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)!=0;
        if(isDir){
            ListView_SetItemText(g_hListView,idx,1,(LPWSTR)L"<DIR>");
        } else {
            wchar_t s[64]; wsprintfW(s,L"%lu KB",fd.nFileSizeLow/1024); ListView_SetItemText(g_hListView,idx,1,s);
        }
        idx++;
    }while(FindNextFileW(h,&fd)); FindClose(h);
}

void NavigateToTypedPath(){
    wchar_t typed[MAX_PATH]; GetWindowTextW(g_hEditPath,typed,MAX_PATH); PathRemoveBlanksW(typed);
    size_t tlen=wcslen(typed); if(tlen>3 && typed[tlen-1]==L'\\') typed[tlen-1]=0;
    if(!PathFileExistsW(typed)){ MessageBoxW(nullptr,typed,L"Folder not found",MB_OK); return; }
    wcscpy_s(g_currentPath,MAX_PATH,typed); SetWindowTextW(g_hEditPath,g_currentPath); RefreshFileList();
}

LRESULT CALLBACK EditSubclassProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam){
    if(msg==WM_KEYDOWN && wParam==VK_RETURN){ NavigateToTypedPath(); return 0; }
    return CallWindowProcW(g_oldEditProc,hWnd,msg,wParam,lParam);
}

LRESULT CALLBACK WndProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam){
 switch(msg){
  case WM_CREATE:{
   InitCommonControls();
   g_hListView=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_VISIBLE|WS_CHILD|LVS_REPORT|LVS_SINGLESEL|WS_BORDER,10,10,760,370,hWnd,(HMENU)ID_LISTVIEW,GetModuleHandleW(nullptr),nullptr);
   ListView_SetExtendedListViewStyle(g_hListView,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
   LVCOLUMNW col={0}; col.mask=LVCF_TEXT|LVCF_WIDTH; col.cx=600; col.pszText=(LPWSTR)L"Name"; ListView_InsertColumn(g_hListView,0,&col);
   col.cx=150; col.pszText=(LPWSTR)L"Size"; ListView_InsertColumn(g_hListView,1,&col);
   g_hEditPath=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",g_currentPath,WS_VISIBLE|WS_CHILD|ES_AUTOHSCROLL,10,395,450,28,hWnd,(HMENU)ID_EDIT_PATH,GetModuleHandleW(nullptr),nullptr);
   g_oldEditProc=(WNDPROC)SetWindowLongPtrW(g_hEditPath,GWLP_WNDPROC,(LONG_PTR)EditSubclassProc);
   CreateWindowW(L"BUTTON",L"Browse...",WS_VISIBLE|WS_CHILD,470,394,90,30,hWnd,(HMENU)ID_BTN_BROWSE,nullptr,nullptr);
   CreateWindowW(L"BUTTON",L"Extract",WS_VISIBLE|WS_CHILD|BS_DEFPUSHBUTTON,570,394,190,30,hWnd,(HMENU)ID_BTN_EXTRACT,nullptr,nullptr);
   g_hStatus=CreateWindowW(L"STATIC",L"Doorzip Extractor - Select .dzi, .dzz, .zd, .doorz to extract",WS_VISIBLE|WS_CHILD,10,435,760,20,hWnd,nullptr,nullptr,nullptr);
   RefreshFileList(); break;
  }
  case WM_COMMAND:{
   int id=LOWORD(wParam);
   if(id==ID_BTN_BROWSE){
    wchar_t picked[MAX_PATH] = L"";
    if(PickFolderModern(hWnd, L"Select folder", picked)){
        wcscpy_s(g_currentPath, MAX_PATH, picked);
        SetWindowTextW(g_hEditPath, g_currentPath);
        RefreshFileList();
    }
   }
   if(id==ID_BTN_EXTRACT){
    int sel=ListView_GetNextItem(g_hListView,-1,LVNI_SELECTED);
    if(sel<=0){ MessageBoxW(hWnd,L"Select an archive file first!",L"Doorzip Extractor",MB_OK); break; }
    wchar_t n[MAX_PATH]; ListView_GetItemText(g_hListView,sel,0,n,MAX_PATH);
    wchar_t fullPath[MAX_PATH]; wsprintfW(fullPath,L"%s\\%s",g_currentPath,n);
    wchar_t destDir[MAX_PATH] = L"";
    if(PickFolderModern(hWnd, L"Select Extraction Destination Folder", destDir)){
        if(ExtractArchive(fullPath, destDir)){
            MessageBoxW(hWnd,L"Extraction successful!",L"Doorzip Extractor",MB_OK);
        } else {
            MessageBoxW(hWnd,L"Extraction failed. Invalid archive or format.",L"Doorzip Extractor",MB_OK);
        }
    }
   }
   break;
  }
  case WM_NOTIFY:{
   LPNMHDR hdr=(LPNMHDR)lParam;
   if(hdr->idFrom==ID_LISTVIEW && hdr->code==NM_DBLCLK){
    int sel=ListView_GetNextItem(g_hListView,-1,LVNI_SELECTED);
    if(sel>=0){
     wchar_t n[MAX_PATH]; ListView_GetItemText(g_hListView,sel,0,n,MAX_PATH);
     if(!wcscmp(n,L"[..]")){
      PathRemoveFileSpecW(g_currentPath); if(!wcslen(g_currentPath)) wcscpy_s(g_currentPath,MAX_PATH,L"C:\\");
     } else {
      wchar_t np[MAX_PATH]; wsprintfW(np,L"%s\\%s",g_currentPath,n);
      if(PathIsDirectoryW(np)) wcscpy_s(g_currentPath,MAX_PATH,np);
     }
     SetWindowTextW(g_hEditPath,g_currentPath); RefreshFileList();
    }
   }
   break;
  }
  case WM_DESTROY: PostQuitMessage(0); break;
  default: return DefWindowProcW(hWnd,msg,wParam,lParam);
 } return 0;
}

int WINAPI wWinMain(HINSTANCE hI,HINSTANCE,LPWSTR,int nCmd){
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    WNDCLASSW wc={0}; wc.lpfnWndProc=WndProc; wc.hInstance=hI; wc.lpszClassName=L"DoorzipExtractor64Class";
    wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    RegisterClassW(&wc);
    HWND w=CreateWindowW(wc.lpszClassName,L"Doorzip Extractor 64-bit",WS_OVERLAPPEDWINDOW,0,0,800,500,nullptr,nullptr,hI,nullptr);
    ShowWindow(w,nCmd);
    MSG m; while(GetMessageW(&m,nullptr,0,0)){ TranslateMessage(&m); DispatchMessageW(&m); }
    CoUninitialize();
    return 0;
}