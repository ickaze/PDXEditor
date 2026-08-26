#pragma once
#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <commdlg.h>
#include <shlwapi.h>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <optional>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <atomic>
#include <memory>

namespace fs = std::filesystem;

inline std::string WToUtf8(const std::wstring& s){ if(s.empty())return{}; int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr); std::string o(n,0); WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),o.data(),n,nullptr,nullptr); return o; }
inline std::wstring Utf8ToW(const std::string& s){ if(s.empty())return{}; int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0); std::wstring o(n,0); MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),o.data(),n); return o; }
inline std::wstring BaseName(const std::wstring& p){ return fs::path(p).filename().wstring(); }
inline uint64_t TickMs(){ return GetTickCount64(); }
