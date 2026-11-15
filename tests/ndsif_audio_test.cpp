// Synthetic receiver tests. No device or third-party firmware is required.
#include "NdsifAudio.h"
#include "NdsifWire.h"
#include <iostream>
namespace nd=mdx::nd;
static void check(bool ok){if(!ok)throw std::runtime_error("NDSIF test failed");}
static nd::B reply;
static uint32_t accepted=0,played=0;
static bool running=false;
static int mode=0;
static std::atomic<int> dataCount{0};
static std::atomic<bool> injectFault{false};
int WriteFile(int,const void* ptr,DWORD n,DWORD* written,void*){
    *written=n;const auto p=static_cast<const uint8_t*>(ptr);nd::B raw;
    check(p[0]==0&&p[n-1]==0&&nd::decode(nd::B(p+1,p+n-1),raw));
    nd::B data(raw.begin()+9,raw.end()-2),r{0};int cmd=raw[4];
    switch(cmd){
    case 0:accepted=played=0;running=false;break;
    case 1:r.insert(r.end(),data.begin(),data.end());break;
    case 2:break;
    case 4:check(data[0]==0xb7&&nd::r32(data,2)==8000000);break;
    case 5:
        nd::u16(r,64);nd::u32(r,256);nd::u16(r,0);nd::u16(r,0);
        nd::u16(r,mode==3?0:65535);r.push_back(1);
        r.insert(r.end(),{uint8_t(mode==1?0x54:0xb7),0,0,uint8_t(mode==2?8:12),uint8_t(mode==2?1:2)});
        nd::u32(r,mode==2?4000000:0);break;
    case 0x10:
        check(data.size()<=64&&nd::r32(data,2)==accepted);
        accepted+=uint32_t(data.size()-6);check(accepted-played<=256);++dataCount;break;
    case 0x12:check(accepted>0&&!running&&nd::r32(data,2)==8000000&&nd::r16(data,6)==512);running=true;break;
    case 0x13:
        if(running)played=std::min(accepted,played+16);
        for(uint32_t x:{accepted,played,accepted-played,0u,0u,0u,0u,uint32_t(injectFault?4:running),0u,0u})nd::u32(r,x);
        break;
    default:check(false);
    }
    if(raw[3]==1)reply=nd::frame(2,cmd,nd::r16(raw,5),r);
    return 1;
}
int ReadFile(int,void* ptr,DWORD n,DWORD* count,void*){
    *count=DWORD(std::min<size_t>({n,reply.size(),7}));std::memcpy(ptr,reply.data(),*count);reply.erase(reply.begin(),reply.begin()+*count);return 1;
}
template<class Predicate>void eventually(Predicate p){
    for(int i=0;i<1000;++i){if(p())return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}check(false);
}
int main(){
    check(nd::crc({'1','2','3','4','5','6','7','8','9'},9)==0x29b1);
    for(unsigned n=0;n<=256;++n){nd::B data(n,0xee),raw;if(n)data[n/2]=0;auto f=nd::frame(1,1,65535,data);nd::B enc(f.begin()+1,f.end()-1);check(nd::decode(enc,raw));check(nd::B(raw.begin()+9,raw.end()-2)==data);enc.back()^=1;check(!nd::decode(enc,raw));}
    auto& out=NdsifAudio::Instance();std::wstring error;
    check(out.Ports()==std::vector<int>({3,12}));
    for(mode=1;mode<=3;++mode){check(!out.Open(3,error));check(!error.empty());}
    mode=0;check(out.Open(3,error));
    std::vector<float> tone(2000);for(size_t i=0;i<tone.size();++i)tone[i]=float(.2*std::sin(i*.1));
    auto a=out.Play(tone,15625),b=out.Play(tone,31250);check(bool(a)&&bool(b));
    eventually([&]{return a->finished.load()&&b->finished.load();});
    check(a->position==2000&&b->position==2000&&dataCount>0);
    auto c=out.Play(tone,15625);c->stopped=true;
    auto d=out.Play(tone,15625);injectFault=true;
    eventually([&]{return d->stopped.load();});check(!out.TakeError().empty());check(!out.Play(tone,15625));
    out.Close();injectFault=false;check(out.Open(12,error));out.Close();
    std::cout<<"PASS: framing, CRC, partial replies, capabilities, clock rejection, queue capacity, polyphony, resampling positions, note stop, fault/reconnect\n";
}
