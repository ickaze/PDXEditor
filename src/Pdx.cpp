#include "Pdx.h"
#include <cmath>
#include "Localization.h"

static uint32_t be32(const uint8_t* p){
    return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];
}
static void putbe32(std::ostream& f,uint32_t v){
    uint8_t b[4]={uint8_t(v>>24),uint8_t(v>>16),uint8_t(v>>8),uint8_t(v)};
    f.write((char*)b,4);
}
static double rough(const std::vector<float>& p){
    if(p.size()<2)return 1e9;
    double s=0; for(size_t i=1;i<p.size();++i)s+=std::abs(p[i]-p[i-1]);
    return s/(p.size()-1);
}

// LZX042-compressed PDX files are intentionally not decoded here.
// This project detects the well-known stream marker only so it can report an
// explicit unsupported-format error without embedding third-party decoder code.
static bool HasLzx042Marker(const std::vector<uint8_t>& src){
    static const uint8_t marker[4]={0x7f,0xff,0xff,0x4c};
    if(src.size()<4)return false;
    for(size_t i=0;i+4<=src.size();++i){
        if(src[i]==marker[0]&&src[i+1]==marker[1]&&src[i+2]==marker[2]&&src[i+3]==marker[3])return true;
    }
    return false;
}

struct PdxCandidate{
    int banks=0;
    bool legacy16=false;
    uint32_t minOff=0xFFFFFFFFu;
    uint32_t gap=0xFFFFFFFFu;
    uint32_t headerBytes=0;
    int samples=0;
    bool exact=false;
};

static bool CheckPdxCandidate(const std::vector<uint8_t>& bytes,int banks,bool legacy16,PdxCandidate& c){
    if(banks<1||banks>256)return false;
    const uint64_t header64=uint64_t(banks)*768ull;
    if(header64>bytes.size()||header64>0xFFFFFFFFull)return false;
    const uint32_t headerSize=uint32_t(header64);

    bool any=false;
    uint32_t minOff=0xFFFFFFFFu;
    int samples=0;
    for(int i=0;i<banks*96;++i){
        const size_t q=size_t(i)*8;
        const uint32_t o=be32(&bytes[q]);
        const uint32_t rawLen=be32(&bytes[q+4]);
        const uint32_t n=legacy16?(rawLen&0xFFFFu):rawLen;

        if(o==0){
            // In legacy PDX, the reserved 16-bit word is not guaranteed to be 0.
            // A zero pointer is therefore the authoritative indication of an empty slot.
            if(!legacy16&&n!=0)return false;
            continue;
        }
        if(n==0||o<headerSize||uint64_t(o)+uint64_t(n)>bytes.size())return false;
        any=true; ++samples; minOff=std::min(minOff,o);
    }

    if(!any){
        if(banks!=1)return false;
        c={banks,legacy16,headerSize,0,headerSize,0,true};
        return true;
    }
    c.banks=banks; c.legacy16=legacy16; c.minOff=minOff;
    c.gap=minOff-headerSize; c.headerBytes=headerSize; c.samples=samples; c.exact=(minOff==headerSize);
    return true;
}

static bool ParsePdxBytes(ImportedPdx& out,std::wstring& err){
    if(out.bytes.size()<8){err=Tr(L"s000");return false;}

    PdxCandidate best{};
    bool found=false;
    const int maxBanks=std::min<int>(256,int(out.bytes.size()/768));

    // EX-PDX: 96 entries per bank, 32-bit offset + 32-bit length.
    // Exact table-to-data boundaries are preferred.  Historical files can have
    // comments/padding between the table and the first sample, so a positive gap
    // is also accepted and the smallest plausible gap is chosen.
    for(int banks=1;banks<=maxBanks;++banks){
        PdxCandidate c;
        if(!CheckPdxCandidate(out.bytes,banks,false,c))continue;
        if(!found||
           (c.exact&&!best.exact)||
           (c.exact==best.exact&&c.gap<best.gap)||
           (c.exact==best.exact&&c.gap==best.gap&&c.banks>best.banks)){
            best=c; found=true;
        }
    }

    // Original PDX compatibility: offset.l + reserved.w + length.w.  Some real
    // PDX files use the reserved word for comments/metadata, which must not become
    // the upper 16 bits of a fictitious 32-bit sample length.
    PdxCandidate legacy{};
    if(CheckPdxCandidate(out.bytes,1,true,legacy)){
        bool preferLegacy=!found;
        if(found){
            if(legacy.samples>best.samples)preferLegacy=true;
            else if(best.banks==1&&legacy.samples==best.samples&&legacy.gap<best.gap)preferLegacy=true;
        }
        if(preferLegacy){best=legacy;found=true;}
    }

    // Last-resort compatibility for malformed but historically distributed PDX
    // files whose header is shorter than a complete 96-entry bank.  Infer the
    // number of header entries from the smallest sample pointer, then expose the
    // missing entries in the final bank as empty slots.
    if(!found){
        auto tryPartial=[&](bool legacy16,PdxCandidate& c)->bool{
            const size_t maxEntries=std::min<size_t>(256u*96u,out.bytes.size()/8u);
            if(maxEntries==0)return false;
            size_t endEntries=maxEntries;
            bool any=false;
            uint32_t minOff=0xFFFFFFFFu;
            int samples=0;
            for(size_t i=0;i<maxEntries && i<endEntries;++i){
                const size_t q=i*8;
                const uint32_t o=be32(&out.bytes[q]);
                const uint32_t rawLen=be32(&out.bytes[q+4]);
                const uint32_t n=legacy16?(rawLen&0xFFFFu):rawLen;
                if(o==0){
                    if(!legacy16&&n!=0)return false;
                    continue;
                }
                if(n==0||uint64_t(o)+uint64_t(n)>out.bytes.size())return false;
                const size_t impliedEntries=size_t(o)/8u;
                if(impliedEntries==0||impliedEntries<=i)return false;
                endEntries=std::min(endEntries,impliedEntries);
                any=true; ++samples; minOff=std::min(minOff,o);
            }
            if(!any||endEntries==0||endEntries>256u*96u)return false;
            const int banks=int((endEntries+95u)/96u);
            const uint32_t logicalHeader=uint32_t(endEntries*8u);
            if(minOff<logicalHeader)return false;
            c.banks=banks; c.legacy16=legacy16; c.minOff=minOff;
            c.gap=minOff-logicalHeader; c.headerBytes=logicalHeader; c.samples=samples; c.exact=(minOff==logicalHeader);
            return true;
        };
        PdxCandidate partial32{},partial16{};
        const bool p32=tryPartial(false,partial32);
        const bool p16=tryPartial(true,partial16);
        if(p32||p16){
            if(p32&&!p16)best=partial32;
            else if(!p32&&p16)best=partial16;
            else if(partial16.samples>partial32.samples)best=partial16;
            else best=partial32;
            found=true;
        }
    }

    if(!found){err=Tr(L"s001");return false;}
    out.banks=best.banks;
    out.table.clear(); out.table.resize(out.banks);

    for(int b=0;b<out.banks;++b){
        for(int i=0;i<96;++i){
            const size_t q=(size_t(b)*96+i)*8;
            auto& e=out.table[b][i];
            if(q+8>out.bytes.size() || q>=best.headerBytes){e.offset=0;e.length=0;continue;}
            e.offset=be32(&out.bytes[q]);
            const uint32_t rawLen=be32(&out.bytes[q+4]);
            e.length=best.legacy16?(rawLen&0xFFFFu):rawLen;
            if(e.offset==0){e.length=0;continue;}
            if(!e.length||e.offset<best.headerBytes||uint64_t(e.offset)+e.length>out.bytes.size()){
                err=Tr(L"s002");return false;
            }
        }
    }

    // PDX does not store the PCM encoding type, so format detection is heuristic.
    // Do not judge from only the first non-empty entry: some real PDX files contain
    // a 1- or 2-byte dummy/silence sample at low note numbers (VATLVA10.PDX is one
    // such case), which makes P8 appear artificially smooth.  Instead, inspect a
    // representative set of sufficiently large samples and compare median roughness.
    struct FormatProbe { const PdxEntry* e=nullptr; };
    std::vector<const PdxEntry*> probes;
    bool p16Possible=true;
    for(const auto& bank:out.table){
        for(const auto& x:bank){
            if(!x.length)continue;
            if(x.length&1u)p16Possible=false;
            if(x.length>=64u)probes.push_back(&x);
        }
    }
    if(probes.empty()){
        for(const auto& bank:out.table)for(const auto& x:bank)if(x.length)probes.push_back(&x);
    }
    std::sort(probes.begin(),probes.end(),[](const PdxEntry* a,const PdxEntry* b){return a->length>b->length;});
    if(probes.size()>16)probes.resize(16);

    auto medianScore=[&](PcmFormat fmt)->double{
        std::vector<double> scores; scores.reserve(probes.size());
        for(const auto* e:probes){
            if(fmt==PcmFormat::P16 && (e->length&1u))continue;
            const auto* d=out.bytes.data()+e->offset;
            const double r=rough(DecodeTarget(d,e->length,fmt));
            if(std::isfinite(r))scores.push_back(r);
        }
        if(scores.empty())return 1e9;
        std::sort(scores.begin(),scores.end());
        const size_t m=scores.size()/2;
        return (scores.size()&1u)?scores[m]:(scores[m-1]+scores[m])*0.5;
    };

    if(!probes.empty()){
        const double a=medianScore(PcmFormat::ADPCM);
        const double p8=medianScore(PcmFormat::P8);
        const double p16=p16Possible?medianScore(PcmFormat::P16):1e9;
        out.guessed=(a<=p8&&a<=p16)?PcmFormat::ADPCM:(p8<=p16?PcmFormat::P8:PcmFormat::P16);
    }
    return true;
}

bool ReadPdx(const std::wstring& path,ImportedPdx& out,std::wstring& err){
    std::ifstream f(fs::path(path),std::ios::binary);
    if(!f){err=Tr(L"s003");return false;}
    out=ImportedPdx{};
    out.bytes.assign(std::istreambuf_iterator<char>(f),{});

    std::wstring normalErr;
    if(ParsePdxBytes(out,normalErr))return true;

    // Compressed PDX is deliberately unsupported.  Detection is performed only
    // after ordinary PDX parsing fails, avoiding false positives from PCM payload.
    if(HasLzx042Marker(out.bytes)){
        err=Tr(L"s004");
        return false;
    }

    err=normalErr;
    return false;
}

bool ExtractPdxAllBanks(const std::wstring&,const ImportedPdx& pdx,const std::wstring& folder,
                        std::vector<std::array<std::wstring,96>>& paths,std::wstring& err){
    try{fs::create_directories(folder);}catch(...){err=Tr(L"s005");return false;}
    paths.clear(); paths.resize(pdx.banks);
    for(int b=0;b<pdx.banks;++b){
        std::wstring bankName=L"bank"; if(b<10)bankName+=L"0"; bankName+=std::to_wstring(b); fs::path bankFolder=fs::path(folder)/bankName;
        try{fs::create_directories(bankFolder);}catch(...){err=Tr(L"s006");return false;}
        for(int i=0;i<96;++i){
            auto e=pdx.table[b][i]; paths[b][i].clear(); if(!e.length)continue;
            auto pcm=DecodeTarget(pdx.bytes.data()+e.offset,e.length,pdx.guessed);
            wchar_t n[16]; swprintf_s(n,L"%02d.wav",i);
            auto p=(bankFolder/n).wstring();
            if(!WriteMono16Wave(p,pcm,15625,err))return false;
            paths[b][i]=p;
        }
    }
    return true;
}

bool WriteExPdxMulti(const std::wstring& path,const std::vector<std::array<std::vector<uint8_t>,96>>& banks,
                     std::wstring& err,std::vector<std::wstring>* log){
    if(banks.empty()||banks.size()>256){err=Tr(L"s007");return false;}
    std::ofstream f(fs::path(path),std::ios::binary);
    if(!f){err=Tr(L"s008");return false;}

    uint64_t off=banks.size()*768ull;
    for(size_t b=0;b<banks.size();++b){
        for(int i=0;i<96;++i){
            uint64_t len=banks[b][i].size();
            if(len>0xFFFFFFFFull){
                len=0xFFFFFFFFull;
                if(log)log->push_back(L"bank "+std::to_wstring(b)+Tr(L"s009")+std::to_wstring(i)+Tr(L"s010"));
            }
            if(off>0xFFFFFFFFull){err=Tr(L"s011");return false;}
            putbe32(f,len?uint32_t(off):0); putbe32(f,uint32_t(len)); off+=len;
        }
    }
    for(const auto& bank:banks){
        for(const auto& s:bank){
            size_t n=std::min<uint64_t>(s.size(),0xFFFFFFFFull);
            if(n)f.write((const char*)s.data(),std::streamsize(n));
        }
    }
    return bool(f);
}
