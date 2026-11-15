#pragma once
#include <cstdint>
#include <cstring>
#include <chrono>
#include <thread>
using DWORD=uint32_t; using HANDLE=int;
constexpr int INVALID_HANDLE_VALUE=-1,GENERIC_READ=1,GENERIC_WRITE=2,OPEN_EXISTING=3,NOPARITY=0,ONESTOPBIT=0,DTR_CONTROL_ENABLE=1,RTS_CONTROL_DISABLE=0,PURGE_RXCLEAR=1,PURGE_TXCLEAR=2;
constexpr DWORD MAXDWORD=UINT32_MAX;
struct DCB {int DCBlength,BaudRate,ByteSize,Parity,StopBits,fBinary,fParity,fOutX,fInX,fOutxCtsFlow,fOutxDsrFlow,fDsrSensitivity,fDtrControl,fRtsControl;};
struct COMMTIMEOUTS {DWORD a,b,c,d,e;};
inline HANDLE CreateFileW(const wchar_t*,int,int,void*,int,int,void*){return 1;}
inline int GetCommState(int,DCB*){return 1;} inline int SetCommState(int,DCB*){return 1;}
inline int SetCommTimeouts(int,COMMTIMEOUTS*){return 1;} inline void CloseHandle(int){}
inline void PurgeComm(int,int){}
inline void Sleep(int ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));}
inline DWORD QueryDosDeviceW(const wchar_t*,wchar_t* p,DWORD){const wchar_t names[]=L"COM3\0COM12\0Other\0";std::memcpy(p,names,sizeof(names));return sizeof(names)/sizeof(wchar_t);}
int WriteFile(int,const void*,DWORD,DWORD*,void*);
int ReadFile(int,void*,DWORD,DWORD*,void*);
