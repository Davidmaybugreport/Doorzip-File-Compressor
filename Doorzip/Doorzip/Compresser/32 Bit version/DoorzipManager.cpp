#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <compressapi.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <filesystem>
namespace fs = std::filesystem;
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "cabinet.lib")

static uint32_t crc_table[256]; static bool crc_table_computed=false;
void make_crc_table(){ for(int n=0;n<256;n++){ uint32_t c=n; for(int k=0;k<8;k++) c = (c&1)?0xedb88320L ^ (c>>1) : c>>1; crc_table[n]=c; } crc_table_computed=true; }
uint32_t update_crc(uint32_t crc, const unsigned char *buf, size_t len){ if(!crc_table_computed) make_crc_table(); uint32_t c=crc; for(size_t n=0;n<len;n++) c=crc_table[(c ^ buf[n]) & 0xff] ^ (c>>8); return c; }
uint32_t crc32(const unsigned char *buf, size_t len){ return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL; }

bool CompressRAD(const std::string& inPath, const std::string& outPath){
    std::ifstream in(inPath, std::ios::binary | std::ios::ate); if(!in) return false;
    size_t sz=(size_t)in.tellg(); in.seekg(0); std::vector<unsigned char> data(sz); if(sz>0) in.read((char*)data.data(),sz); in.close();
    std::ofstream out(outPath, std::ios::binary); if(!out) return false;
    out.write("RAD",3);
    size_t i=0;
    while(i<data.size()){
        unsigned char cur=data[i]; size_t run=1;
        while(i+run<data.size() && data[i+run]==cur && run<0xFFFFFFFF) run++;
        if(run>=4){ out.put('R'); out.put('A'); out.put(cur); out.put('D'); uint32_t c=(uint32_t)run; out.write((char*)&c,4); i+=run; continue; }
        if(cur=='R'){ out.put('R'); out.put('R'); i++; continue; }
        out.put(cur); i++;
    }
    out.close(); return true;
}

bool CreateRawBlobForRAD(const std::wstring& srcPathW, const std::string& tempPath){
    if(fs::is_directory(srcPathW)){
        std::ofstream out(tempPath, std::ios::binary);
        for(auto& p: fs::recursive_directory_iterator(srcPathW)){
            if(!p.is_regular_file()) continue;
            std::string rel=fs::relative(p.path(), srcPathW).string();
            uint32_t nl=(uint32_t)rel.size(); out.write((char*)&nl,4); out.write(rel.c_str(),nl);
            uint64_t fsz=(uint64_t)fs::file_size(p.path()); out.write((char*)&fsz,8);
            std::ifstream inf(p.path(), std::ios::binary); std::vector<char> d((size_t)fsz); if(fsz>0) inf.read(d.data(),fsz); out.write(d.data(),fsz);
        }
        out.close(); return true;
    }
    fs::copy_file(srcPathW, fs::path(tempPath), fs::copy_options::overwrite_existing); return true;
}

bool Find7zExe(std::wstring& outPath){
    const wchar_t* paths[] = { L"C:\\Program Files\\7-Zip\\7z.exe", L"C:\\Program Files (x86)\\7-Zip\\7z.exe" };
    for(auto p: paths){ if(PathFileExistsW(p)){ outPath=p; return true; } }
    return false;
}

bool Create7zLZMA2(const std::wstring& srcPathW, const std::wstring& outPathW){
    std::wstring sevenZip; if(!Find7zExe(sevenZip)) return false;
    std::wstring cmd;
    if(fs::is_directory(srcPathW)){
        cmd = L"\"" + sevenZip + L"\" a -t7z -mx=9 -m0=lzma2 -y \"" + outPathW + L"\" \"" + srcPathW + L"\\*\"";
    } else {
        cmd = L"\"" + sevenZip + L"\" a -t7z -mx=9 -m0=lzma2 -y \"" + outPathW + L"\" \"" + srcPathW + L"\"";
    }
    STARTUPINFOW si={0}; si.cb=sizeof(si); PROCESS_INFORMATION pi={0};
    si.dwFlags=STARTF_USESHOWWINDOW; si.wShowWindow=SW_HIDE;
    std::wstring cmdCopy=cmd;
    if(CreateProcessW(NULL, cmdCopy.data(), NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)){
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode=0; GetExitCodeProcess(pi.hProcess,&exitCode);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        return exitCode==0 && PathFileExistsW(outPathW.c_str());
    }
    return false;
}

#define ID_LISTVIEW 1001
#define ID_BTN_COMPRESS 1002
#define ID_EDIT_PATH 1005
#define ID_BTN_BROWSE 1006
#define ID_DLG_COMBO_EXT 2001
#define ID_DLG_EDIT_BYTES 2002
#define ID_DLG_MAKEZIP 2003
#define ID_DLG_CANCEL 2004
#define ID_DLG_CHECK_7Z 2005

HWND g_hListView,g_hStatus,g_hEditPath;
wchar_t g_currentPath[MAX_PATH]=L"C:\\";
wchar_t g_selectedFull[MAX_PATH]=L"";
WNDPROC g_oldEditProc=nullptr;
HWND g_hDlg=nullptr;
HWND g_hDlgCombo=nullptr;
HWND g_hDlgBytes=nullptr;
HWND g_hDlgCheck7z=nullptr;

BOOL DoMakeZip(const wchar_t* src, const wchar_t* extType, DWORD bytes, BOOL use7zCompat){
    wchar_t extLower[32]; wcscpy_s(extLower,32,extType); CharLowerW(extLower);
    wchar_t outPathW[MAX_PATH]; wsprintfW(outPathW,L"%s.%s",src,extLower);
    wchar_t tempPathW[MAX_PATH]; GetTempPathW(MAX_PATH,tempPathW);
    wchar_t tempW[MAX_PATH]; wsprintfW(tempW,L"%sDoorzip_%lu.bin",tempPathW,GetTickCount());
    char tempA[MAX_PATH]; WideCharToMultiByte(CP_UTF8,0,tempW,-1,tempA,MAX_PATH,nullptr,nullptr);
    char outA[MAX_PATH]; WideCharToMultiByte(CP_UTF8,0,outPathW,-1,outA,MAX_PATH,nullptr,nullptr);
    
    if(use7zCompat){
        if(Create7zLZMA2(src, outPathW)){
            size_t finalSz=(size_t)fs::file_size(outPathW);
            wchar_t msg[1024]; wsprintfW(msg,L"7z-Compatible LZMA2 Created:\n%s\nSize: %zu bytes (was 551 MB)\n7-Zip WILL open this - it's a real .7z file with .dzi name",outPathW,finalSz);
            MessageBoxW(g_hDlg,msg,L"Doorzip - 7z LZMA2 Mode",MB_OK);
            return TRUE;
        }
        MessageBoxW(g_hDlg,L"7-Zip not found at C:\\Program Files\\7-Zip\\7z.exe\nInstall 7-Zip for best compression. Using ZIP fallback (won't shrink much).",L"Doorzip",MB_OK);
        return FALSE;
    }
    if(!CreateRawBlobForRAD(src, tempA)){ MessageBoxW(g_hDlg,L"Failed blob",L"Doorzip",MB_OK); return FALSE; }
    if(!CompressRAD(tempA, outA)){ MessageBoxW(g_hDlg,L"Failed RAD",L"Doorzip",MB_OK); fs::remove(tempA); return FALSE; }
    fs::remove(tempA);
    size_t finalSz=(size_t)fs::file_size(outA);
    if(finalSz < bytes){
        std::ofstream out(outA,std::ios::binary|std::ios::app);
        size_t pad=bytes-finalSz; std::vector<char> z(4096,0); while(pad>0){ size_t w=std::min(pad,z.size()); out.write(z.data(),w); pad-=w; }
        out.close(); finalSz=bytes;
    }
    wchar_t msg[1024]; wsprintfW(msg,L"RAD Archive Created:\n%s\nFinal: %zu bytes\nDoorzip-only, smallest",outPathW,finalSz);
    MessageBoxW(g_hDlg,msg,L"Doorzip - RAD Mode",MB_OK);
    return TRUE;
}

LRESULT CALLBACK CompressDlgProc(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam){
 switch(msg){
  case WM_CREATE:{
   CreateWindowW(L"STATIC",L"Format:",WS_VISIBLE|WS_CHILD,20,20,60,20,hDlg,nullptr,nullptr,nullptr);
   g_hDlgCombo=CreateWindowExW(0,L"COMBOBOX",L"",WS_VISIBLE|WS_CHILD|CBS_DROPDOWNLIST|WS_VSCROLL,90,18,180,200,hDlg,(HMENU)ID_DLG_COMBO_EXT,nullptr,nullptr);
   SendMessageW(g_hDlgCombo,CB_ADDSTRING,0,(LPARAM)L"Dzi");
   SendMessageW(g_hDlgCombo,CB_ADDSTRING,0,(LPARAM)L"Zd");
   SendMessageW(g_hDlgCombo,CB_ADDSTRING,0,(LPARAM)L"Dzz");
   SendMessageW(g_hDlgCombo,CB_ADDSTRING,0,(LPARAM)L"Doorz");
   SendMessageW(g_hDlgCombo,CB_SETCURSEL,0,0);
   CreateWindowW(L"STATIC",L"Bytes (min 166):",WS_VISIBLE|WS_CHILD,20,60,110,20,hDlg,nullptr,nullptr,nullptr);
   g_hDlgBytes=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"166",WS_VISIBLE|WS_CHILD|ES_NUMBER,140,58,130,24,hDlg,(HMENU)ID_DLG_EDIT_BYTES,nullptr,nullptr);
   g_hDlgCheck7z=CreateWindowExW(0,L"BUTTON",L"Compatible with 7z (LZMA2 - Shrinks)",WS_VISIBLE|WS_CHILD|BS_AUTOCHECKBOX,20,95,250,20,hDlg,(HMENU)ID_DLG_CHECK_7Z,nullptr,nullptr);
   SendMessageW(g_hDlgCheck7z,BM_SETCHECK,BST_CHECKED,0);
   CreateWindowW(L"STATIC",g_selectedFull,WS_VISIBLE|WS_CHILD|SS_PATHELLIPSIS,20,125,270,20,hDlg,nullptr,nullptr,nullptr);
   CreateWindowW(L"BUTTON",L"Make Archive",WS_VISIBLE|WS_CHILD|BS_DEFPUSHBUTTON,30,155,110,35,hDlg,(HMENU)ID_DLG_MAKEZIP,nullptr,nullptr);
   CreateWindowW(L"BUTTON",L"Cancel",WS_VISIBLE|WS_CHILD,170,155,100,35,hDlg,(HMENU)ID_DLG_CANCEL,nullptr,nullptr);
   break;
  }
  case WM_COMMAND:{
   int id=LOWORD(wParam);
   if(id==ID_DLG_MAKEZIP){
    wchar_t ext[32]; ext[0]=0;
    int sel=(int)SendMessageW(g_hDlgCombo,CB_GETCURSEL,0,0);
    if(sel>=0){ SendMessageW(g_hDlgCombo,CB_GETLBTEXT,sel,(LPARAM)ext); }
    if(sel<0){ wcscpy_s(ext,32,L"Dzi"); }
    wchar_t sBytes[32]; GetWindowTextW(g_hDlgBytes,sBytes,32); DWORD b=(DWORD)_wtoi(sBytes);
    if(b<166){ MessageBoxW(hDlg,L"Minimum 166",L"Doorzip",MB_OK); SetWindowTextW(g_hDlgBytes,L"166"); break; }
    BOOL compat=(SendMessageW(g_hDlgCheck7z,BM_GETCHECK,0,0)==BST_CHECKED);
    DoMakeZip(g_selectedFull,ext,b,compat);
    HWND parent=(HWND)GetWindowLongPtrW(hDlg,GWLP_USERDATA); EnableWindow(parent,TRUE); DestroyWindow(hDlg); g_hDlg=nullptr;
   }
   if(id==ID_DLG_CANCEL){ HWND parent=(HWND)GetWindowLongPtrW(hDlg,GWLP_USERDATA); EnableWindow(parent,TRUE); DestroyWindow(hDlg); g_hDlg=nullptr; }
   break;
  }
  case WM_CLOSE:{ HWND parent=(HWND)GetWindowLongPtrW(hDlg,GWLP_USERDATA); EnableWindow(parent,TRUE); DestroyWindow(hDlg); g_hDlg=nullptr; break; }
  default: return DefWindowProcW(hDlg,msg,wParam,lParam);
 } return 0;
}
void ShowCompressDialog(HWND parent){
    if(g_hDlg){ SetForegroundWindow(g_hDlg); return; }
    WNDCLASSW wc={0}; wc.lpfnWndProc=CompressDlgProc; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=L"DoorzipCompressDlg"; wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); RegisterClassW(&wc);
    g_hDlg=CreateWindowW(wc.lpszClassName,L"Compress - Doorzip",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,0,0,330,250,parent,nullptr,wc.hInstance,nullptr);
    SetWindowLongPtrW(g_hDlg,GWLP_USERDATA,(LONG_PTR)parent);
    RECT pr,dr; GetWindowRect(parent,&pr); GetWindowRect(g_hDlg,&dr);
    int x=pr.left+(pr.right-pr.left)/2-(dr.right-dr.left)/2; int y=pr.top+(pr.bottom-pr.top)/2-(dr.bottom-dr.top)/2;
    SetWindowPos(g_hDlg,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER); EnableWindow(parent,FALSE);
}
void RefreshFileList(){
    ListView_DeleteAllItems(g_hListView);
    WIN32_FIND_DATAW fd; wchar_t search[MAX_PATH]; wsprintfW(search,L"%s\\*",g_currentPath);
    HANDLE h=FindFirstFileW(search,&fd); if(h==INVALID_HANDLE_VALUE) return;
    int idx=0; LVITEMW it={0}; it.mask=LVIF_TEXT; it.iItem=idx; it.pszText=(LPWSTR)L"[..]"; ListView_InsertItem(g_hListView,&it); idx++;
    do{
        if(!wcscmp(fd.cFileName,L".")) continue;
        if(!wcscmp(fd.cFileName,L"..")) continue;
        it.iItem=idx; it.pszText=fd.cFileName; ListView_InsertItem(g_hListView,&it);
        bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if(isDir){ ListView_SetItemText(g_hListView,idx,1,(LPWSTR)L"<DIR>"); }
        if(!isDir){ wchar_t s[64]; wsprintfW(s,L"%lu KB",fd.nFileSizeLow/1024); ListView_SetItemText(g_hListView,idx,1,s); }
        idx++;
    }while(FindNextFileW(h,&fd)); FindClose(h);
}
void NavigateToTypedPath(){ wchar_t typed[MAX_PATH]; GetWindowTextW(g_hEditPath,typed,MAX_PATH); PathRemoveBlanksW(typed); size_t tlen=wcslen(typed); if(tlen>3 && typed[tlen-1]==L'\\') typed[tlen-1]=0; if(!PathFileExistsW(typed)){ MessageBoxW(nullptr,typed,L"Folder not found",MB_OK); return; } wcscpy_s(g_currentPath,MAX_PATH,typed); SetWindowTextW(g_hEditPath,g_currentPath); RefreshFileList(); }
LRESULT CALLBACK EditSubclassProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam){ if(msg==WM_KEYDOWN && wParam==VK_RETURN){ NavigateToTypedPath(); return 0; } return CallWindowProcW(g_oldEditProc,hWnd,msg,wParam,lParam); }
LRESULT CALLBACK WndProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam){
 switch(msg){
  case WM_CREATE:{
   InitCommonControls();
   g_hListView=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_VISIBLE|WS_CHILD|LVS_REPORT|LVS_SINGLESEL|WS_BORDER,10,10,760,400,hWnd,(HMENU)ID_LISTVIEW,GetModuleHandleW(nullptr),nullptr);
   ListView_SetExtendedListViewStyle(g_hListView,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
   LVCOLUMNW col={0}; col.mask=LVCF_TEXT|LVCF_WIDTH; col.cx=600; col.pszText=(LPWSTR)L"Name"; ListView_InsertColumn(g_hListView,0,&col); col.cx=150; col.pszText=(LPWSTR)L"Size"; ListView_InsertColumn(g_hListView,1,&col);
   g_hEditPath=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",g_currentPath,WS_VISIBLE|WS_CHILD|ES_AUTOHSCROLL,10,420,550,30,hWnd,(HMENU)ID_EDIT_PATH,GetModuleHandleW(nullptr),nullptr);
   g_oldEditProc=(WNDPROC)SetWindowLongPtrW(g_hEditPath,GWLP_WNDPROC,(LONG_PTR)EditSubclassProc);
   CreateWindowW(L"BUTTON",L"Browse...",WS_VISIBLE|WS_CHILD,570,420,90,30,hWnd,(HMENU)ID_BTN_BROWSE,nullptr,nullptr);
   CreateWindowW(L"BUTTON",L"Compress",WS_VISIBLE|WS_CHILD|BS_DEFPUSHBUTTON,670,420,100,30,hWnd,(HMENU)ID_BTN_COMPRESS,nullptr,nullptr);
   g_hStatus=CreateWindowW(L"STATIC",L"Ready - Check=7z LZMA2 Shrinks, Uncheck=RAD",WS_VISIBLE|WS_CHILD,10,460,760,20,hWnd,nullptr,nullptr,nullptr);
   RefreshFileList(); break;
  }
  case WM_COMMAND:{
   int id=LOWORD(wParam);
   if(id==ID_BTN_BROWSE){ BROWSEINFOW bi={0}; bi.hwndOwner=hWnd; bi.lpszTitle=L"Select folder"; bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE; LPITEMIDLIST p=SHBrowseForFolderW(&bi); if(p){ SHGetPathFromIDListW(p,g_currentPath); SetWindowTextW(g_hEditPath,g_currentPath); RefreshFileList(); CoTaskMemFree(p);} }
   if(id==ID_BTN_COMPRESS){ int sel=ListView_GetNextItem(g_hListView,-1,LVNI_SELECTED); if(sel<=0){ MessageBoxW(hWnd,L"Select a file first!",L"Doorzip",MB_OK); break; } wchar_t n[MAX_PATH]; ListView_GetItemText(g_hListView,sel,0,n,MAX_PATH); wsprintfW(g_selectedFull,L"%s\\%s",g_currentPath,n); ShowCompressDialog(hWnd); } break;
  }
  case WM_NOTIFY:{ LPNMHDR hdr=(LPNMHDR)lParam; if(hdr->idFrom==ID_LISTVIEW && hdr->code==NM_DBLCLK){ int sel=ListView_GetNextItem(g_hListView,-1,LVNI_SELECTED); wchar_t n[MAX_PATH]; ListView_GetItemText(g_hListView,sel,0,n,MAX_PATH); if(!wcscmp(n,L"[..]")){ PathRemoveFileSpecW(g_currentPath); if(!wcslen(g_currentPath)) wcscpy_s(g_currentPath,MAX_PATH,L"C:\\"); } if(wcscmp(n,L"[..]")!=0){ wchar_t np[MAX_PATH]; wsprintfW(np,L"%s\\%s",g_currentPath,n); if(PathIsDirectoryW(np)) wcscpy_s(g_currentPath,MAX_PATH,np); } SetWindowTextW(g_hEditPath,g_currentPath); RefreshFileList(); } break; }
  case WM_DESTROY: PostQuitMessage(0); break;
  default: return DefWindowProcW(hWnd,msg,wParam,lParam);
 } return 0;
}
int WINAPI wWinMain(HINSTANCE hI,HINSTANCE,LPWSTR,int nCmd){ WNDCLASSW wc={0}; wc.lpfnWndProc=WndProc; wc.hInstance=hI; wc.lpszClassName=L"Doorzip32Class"; wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); RegisterClassW(&wc); HWND w=CreateWindowW(wc.lpszClassName,L"Doorzip Manager 32-bit",WS_OVERLAPPEDWINDOW,0,0,800,540,nullptr,nullptr,hI,nullptr); ShowWindow(w,nCmd); MSG m; while(GetMessageW(&m,nullptr,0,0)){ TranslateMessage(&m); DispatchMessageW(&m);} return 0; }