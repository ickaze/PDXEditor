#pragma once
#include "Common.h"

struct LanguagePack {
    std::wstring code;
    std::wstring name;
    std::map<std::wstring,std::wstring> text; // English source text -> localized text
};

inline std::vector<LanguagePack> gLanguagePacks;
inline int gLanguageIndex = 0;
inline bool gEnglishUi = false; // compatibility: true when selected pack code is en

inline std::wstring LangUnescape(const std::string& s){
    std::string o;
    for(size_t i=0;i<s.size();++i){
        if(s[i]=='\\' && i+1<s.size()){
            char c=s[++i];
            if(c=='n') o+='\n'; else if(c=='r') o+='\r'; else if(c=='t') o+='\t'; else o+=c;
        }else o+=s[i];
    }
    return Utf8ToW(o);
}
inline std::string LangTrim(std::string s){
    while(!s.empty() && (s.back()=='\r'||s.back()=='\n'||s.back()==' '||s.back()=='\t'))s.pop_back();
    size_t i=0;while(i<s.size()&&(s[i]==' '||s[i]=='\t'))++i;return s.substr(i);
}
inline bool LoadLanguagePackFile(const fs::path& path, LanguagePack& out){
    std::ifstream f(path,std::ios::binary); if(!f)return false;
    std::string line; bool first=true;
    while(std::getline(f,line)){
        if(first && line.size()>=3 && (unsigned char)line[0]==0xEF && (unsigned char)line[1]==0xBB && (unsigned char)line[2]==0xBF) line.erase(0,3);
        first=false; if(line.empty()||line[0]=='#'||line[0]==';')continue;
        auto q=line.find('='); if(q==std::string::npos)continue;
        auto k=LangTrim(line.substr(0,q)),v=line.substr(q+1);
        if(k=="code")out.code=LangUnescape(v);
        else if(k=="name")out.name=LangUnescape(v);
        else if(k.rfind("text.",0)==0)out.text[LangUnescape(k.substr(5))]=LangUnescape(v);
    }
    if(out.code.empty())out.code=path.stem().wstring(); if(out.name.empty())out.name=out.code;
    return !out.text.empty();
}
inline bool IsWindowsUiJapanese(){LANGID lang=GetUserDefaultUILanguage();return PRIMARYLANGID(lang)==LANG_JAPANESE;}
inline void LoadLanguagePacks(const fs::path& appDir){
    gLanguagePacks.clear();
    fs::path dir=appDir/L"languages";
    try{if(fs::exists(dir))for(auto& e:fs::directory_iterator(dir))if(e.is_regular_file()&& !_wcsicmp(e.path().extension().c_str(),L".lang")){LanguagePack p;if(LoadLanguagePackFile(e.path(),p))gLanguagePacks.push_back(std::move(p));}}catch(...){}
    std::stable_sort(gLanguagePacks.begin(),gLanguagePacks.end(),[](auto&a,auto&b){auto rank=[](const std::wstring&c){return !_wcsicmp(c.c_str(),L"ja")?0:(!_wcsicmp(c.c_str(),L"en")?1:2);};int ra=rank(a.code),rb=rank(b.code);return ra!=rb?ra<rb:a.name<b.name;});
    if(gLanguagePacks.empty()){
        LanguagePack p;p.code=L"en";p.name=L"English";gLanguagePacks.push_back(std::move(p));
    }
}
inline int LanguageIndexByCode(const std::wstring& code){for(size_t i=0;i<gLanguagePacks.size();++i)if(!_wcsicmp(gLanguagePacks[i].code.c_str(),code.c_str()))return(int)i;return-1;}
inline void SelectLanguageIndex(int i){if(i<0||i>=(int)gLanguagePacks.size())i=0;gLanguageIndex=i;gEnglishUi=!_wcsicmp(gLanguagePacks[i].code.c_str(),L"en");}
inline const wchar_t* Tr(const wchar_t* key){
    if(gLanguageIndex>=0&&gLanguageIndex<(int)gLanguagePacks.size()){
        auto it=gLanguagePacks[gLanguageIndex].text.find(key);if(it!=gLanguagePacks[gLanguageIndex].text.end())return it->second.c_str();
    }
    int en=LanguageIndexByCode(L"en");if(en>=0){auto it=gLanguagePacks[en].text.find(key);if(it!=gLanguagePacks[en].text.end())return it->second.c_str();}
    return key;
}
