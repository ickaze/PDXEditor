#!/usr/bin/env python3
"""Build the actual portable core with test-only substitutes for Windows APIs.
Requires Python 3 and g++ with C++17; does not test Windows GUI or waveOut.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]

def read(name):
    return (ROOT / name).read_text(encoding="utf-8-sig")

def strip_includes(text):
    return "\n".join(line for line in text.splitlines()
                     if not line.startswith("#include") and line != "#pragma once")

pre = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>
namespace fs=std::filesystem;
const int CW_USEDEFAULT=-2147483647;
const int MOVEFILE_REPLACE_EXISTING=1,MOVEFILE_WRITE_THROUGH=2;
using LANGID=unsigned short;
const int LANG_JAPANESE=17;
LANGID GetUserDefaultUILanguage(){return 9;}
int PRIMARYLANGID(LANGID l){return l&1023;}
int _wcsicmp(const wchar_t*a,const wchar_t*b){
 std::wstring x(a),y(b);for(auto&c:x)c=std::towlower(c);for(auto&c:y)c=std::towlower(c);
 return x.compare(y);
}
int _wcsicmp(const char*a,const wchar_t*b){std::string s(a);std::wstring w(s.begin(),s.end());return _wcsicmp(w.c_str(),b);}
std::string WToUtf8(const std::wstring&s){return {s.begin(),s.end()};}
std::wstring Utf8ToW(const std::string&s){return {s.begin(),s.end()};}
bool MoveFileExW(const char*a,const char*b,int){std::error_code e;fs::rename(a,b,e);return !e;}
template<size_t N,class...Args>int swprintf_s(wchar_t(&b)[N],const wchar_t*f,Args...a){return std::swprintf(b,N,f,a...);}
'''
loc = strip_includes(read("src/Localization.h"))
pos = loc.index("inline std::vector<LanguagePack>")
loc = loc[:pos] + strip_includes(read("src/BuiltinEnglish.h")) + loc[pos:]
audio_h = strip_includes(read("src/Audio.h")).split("class WaveOutPlayer")[0]
audio = strip_includes(read("src/Audio.cpp")).split("void WaveOutPlayer::Stop")[0]
code = (pre + loc + audio_h + strip_includes(read("src/Pdx.h"))
        + strip_includes(read("src/Settings.h")) + audio
        + strip_includes(read("src/Pdx.cpp")) + strip_includes(read("src/Settings.cpp")))
main = r'''
int main(int argc,char**argv){
 fs::path d=argv[1];std::wstring err,last;std::string saved;
 LoadLanguagePacks(d/"missing");SelectLanguageIndex(LanguageIndexByCode(L"en"));
 assert(std::wstring(Tr(L"s044"))!=L"s044");
 assert(BuiltinEnglishLanguage().text.size()>=100);
 ProjectSetting p;p.pdxName=L"sample";p.banks[0].slot[0].reverse=true;
 p.banks[0].slot[0].trimStart=2;p.windowW=1200;p.languageCode=L"en";
 assert(SaveProject((d/"sample.pdxedit").wstring(),p,err));
 ProjectSetting q;assert(LoadProject((d/"sample.pdxedit").wstring(),q,err));
 assert(q.banks[0].slot[0].reverse&&q.banks[0].slot[0].trimStart==2);
 assert(SaveEditorSettings((d/"PDXEditor.ini").wstring(),p,(d/"sample.pdxedit").wstring(),saved));
 assert(LoadEditorSettings((d/"PDXEditor.ini").wstring(),q,last));assert(q.windowW==1200);
 {std::ofstream f(d/"partial.ini");f<<"windowW=1300\n";}
 std::set<std::string> iniKeys,projectKeys;ProjectSetting common,legacy;
 assert(LoadEditorSettings((d/"partial.ini").wstring(),common,last,&iniKeys));
 {std::ofstream f(d/"legacy.pdxedit");f<<"pdxName=legacy\nwindowW=1600\nwindowH=900\n";}
 assert(LoadProject((d/"legacy.pdxedit").wstring(),legacy,err,&projectKeys));
 MergeEditorSettings(legacy,common,iniKeys,projectKeys);assert(legacy.windowW==1300&&legacy.windowH==900);
 std::vector<float> samples{0.0f,0.25f,-0.25f,0.5f,-0.5f,0.0f};
 assert(WriteMono16Wave((d/"test.wav").wstring(),samples,15625,err));WaveData wave;
 assert(LoadWave((d/"test.wav").wstring(),wave,err));assert(wave.mono.size()==samples.size());
 for(PcmFormat fmt:{PcmFormat::ADPCM,PcmFormat::P16,PcmFormat::P8}){
  auto data=EncodeTarget(samples,fmt);auto decoded=DecodeTarget(data.data(),data.size(),fmt);
  assert(!data.empty()&&decoded.size()==samples.size());
 }
 std::vector<std::array<std::vector<uint8_t>,96>> banks(3);
 banks[0][0]=EncodeTarget(samples,PcmFormat::P16);banks[2][4]=banks[0][0];
 assert(WriteExPdxMulti((d/"multi.pdx").wstring(),banks,err));ImportedPdx pdx;
 assert(ReadPdx((d/"multi.pdx").wstring(),pdx,err));assert(pdx.banks==3);
 assert(pdx.table[1][0].length==0&&pdx.table[2][4].length==banks[2][4].size());
 banks.resize(1);assert(WriteExPdxMulti((d/"single.pdx").wstring(),banks,err));
 assert(ReadPdx((d/"single.pdx").wstring(),pdx,err)&&pdx.banks==1);
 std::cout<<"portable core: passed\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
    d = Path(tmp)
    (d / "checks.cpp").write_text(code + main, encoding="utf-8")
    subprocess.run(["g++", "-std=c++17", "-O2", str(d / "checks.cpp"), "-o", str(d / "checks")], check=True)
    subprocess.run([str(d / "checks"), tmp], check=True)

# Check every used string ID is supplied in both external language files.
keys = set()
for p in (ROOT / "src").glob("*"):
    if p.suffix in {".cpp", ".h"} and p.name != "BuiltinEnglish.h":
        keys.update(re.findall(r'Tr\(L"(s\d+)"\)', p.read_text(encoding="utf-8-sig")))
for lang in ("ja", "en"):
    supplied = set(re.findall(r'^text\.(s\d+)=', read(f"languages/{lang}.lang"), re.M))
    assert keys <= supplied, (lang, keys - supplied)
ET.parse(ROOT / "PDXEditor.vcxproj")
assert 'AfterTargets="Build"' in read("PDXEditor.vcxproj")
assert 'POST_BUILD' in read("CMakeLists.txt")
assert 'TimeStretchOLA' not in read("src/Audio.cpp")
print("language coverage and build metadata: passed")
