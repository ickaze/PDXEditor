#include "App.h"

int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR,int){
    std::wstring startupProject;
    int argc=0;
    LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(argv){
        if(argc>=2) startupProject=argv[1];
        LocalFree(argv);
    }
    App a;
    return a.Run(h,startupProject);
}
