// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 IndependentMDX contributors
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace mdx::nd {
using B=std::vector<uint8_t>;
inline void u16(B& b,uint16_t v){b.push_back(uint8_t(v));b.push_back(uint8_t(v>>8));}
inline void u32(B& b,uint32_t v){u16(b,uint16_t(v));u16(b,uint16_t(v>>16));}
inline unsigned r16(const B& b,size_t n){return b.at(n)|(unsigned(b.at(n+1))<<8);}
inline uint32_t r32(const B& b,size_t n){return r16(b,n)|(uint32_t(r16(b,n+2))<<16);}
inline uint16_t crc(const B& b,size_t len){
    uint16_t c=0xffff; for(size_t i=0;i<len;++i){c^=uint16_t(b[i])<<8;for(int j=0;j<8;++j)c=uint16_t((c<<1)^((c&0x8000)?0x1021:0));}return c;
}
inline B frame(int type,int cmd,uint16_t id,const B& payload){
    if(payload.size()>256)throw std::runtime_error("NDSIF: oversized payload");
    B raw={'N','D',1,uint8_t(type),uint8_t(cmd)};u16(raw,id);u16(raw,uint16_t(payload.size()));
    raw.insert(raw.end(),payload.begin(),payload.end());u16(raw,crc(raw,raw.size()));
    B out={0,0};size_t code=1;
    for(auto v:raw){if(!v){out[code]=uint8_t(out.size()-code);code=out.size();out.push_back(0);}
        else{out.push_back(v);if(out.size()-code==255){out[code]=255;code=out.size();out.push_back(0);}}}
    out[code]=uint8_t(out.size()-code);out.push_back(0);return out;
}
inline bool decode(const B& encoded,B& out){
    out.clear();if(encoded.empty()||encoded.size()>269)return false;
    for(size_t i=0;i<encoded.size();){unsigned n=encoded[i++];if(!n||i+n-1>encoded.size())return false;
        out.insert(out.end(),encoded.begin()+i,encoded.begin()+i+n-1);i+=n-1;if(n!=255&&i<encoded.size())out.push_back(0);}
    return out.size()>=11&&out.size()<=267&&out[0]=='N'&&out[1]=='D'&&out[2]==1&&r16(out,7)+11==out.size()&&r16(out,out.size()-2)==crc(out,out.size()-2);
}
// OKI 12-bit predictor; one state per continuous device stream.
struct Encoder {
    int predictor=-2,index=0,low=-1;
    uint8_t encode(int target){
        static const auto step=[] {std::array<int,49> a{};for(int i=0;i<49;++i)a[i]=int(16*std::pow(1.1,i));return a;}();
        static constexpr int change[]={-1,-1,-1,-1,2,4,6,8};
        int s=step[index],best=0,error=100000,next=predictor;
        for(int n=0;n<16;++n){int d=s/8+((n&1)?s/4:0)+((n&2)?s/2:0)+((n&4)?s:0);
            int p=std::clamp(predictor+((n&8)?-d:d),-2048,2047);
            if(std::abs(target-p)<error){best=n;error=std::abs(target-p);next=p;}}
        predictor=next;index=std::clamp(index+change[best&7],0,48);return uint8_t(best);
    }
    void sample(int target,B& bytes){int n=encode(target);if(low<0)low=n;else{bytes.push_back(uint8_t(low|(n<<4)));low=-1;}}
};
}
