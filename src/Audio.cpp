#include "Audio.h"
#include "Localization.h"

static uint16_t rd16(const uint8_t*p){return uint16_t(p[0]|(p[1]<<8));}
static uint32_t rd32(const uint8_t*p){return uint32_t(p[0]|(p[1]<<8)|(p[2]<<16)|(p[3]<<24));}

bool LoadWave(const std::wstring& path, WaveData& out, std::wstring& err){
  std::ifstream f(fs::path(path),std::ios::binary); if(!f){err=Tr(L"s012");return false;}
  std::vector<uint8_t>d((std::istreambuf_iterator<char>(f)),{}); if(d.size()<12||memcmp(d.data(),"RIFF",4)||memcmp(d.data()+8,"WAVE",4)){err=Tr(L"s013");return false;}
  uint16_t tag=0,ch=0,bits=0,block=0; uint32_t rate=0; const uint8_t* pcm=nullptr; size_t pcmBytes=0;
  for(size_t p=12;p+8<=d.size();){ uint32_t n=rd32(&d[p+4]); size_t b=p+8; if(b+n>d.size())break;
    if(!memcmp(&d[p],"fmt ",4)&&n>=16){tag=rd16(&d[b]);ch=rd16(&d[b+2]);rate=rd32(&d[b+4]);block=rd16(&d[b+12]);bits=rd16(&d[b+14]); if(tag==0xFFFE&&n>=40) tag=rd16(&d[b+24]);}
    else if(!memcmp(&d[p],"data",4)){pcm=&d[b];pcmBytes=n;}
    p=b+n+(n&1);
  }
  if(!pcm||!ch||!rate||!block){err=Tr(L"s014");return false;}
  if(tag!=1&&tag!=3){err=Tr(L"s015");return false;}
  size_t frames=pcmBytes/block; out.mono.resize(frames); out.sampleRate=rate; out.bits=bits; out.channels=ch;
  out.formatName=(tag==3?L"WAV Float":L"WAV PCM");
  for(size_t i=0;i<frames;i++){
    double sum=0; const uint8_t* fr=pcm+i*block;
    for(uint16_t c=0;c<ch;c++){
      const uint8_t* q=fr+c*(bits/8); double v=0;
      if(tag==3&&bits==32){float x; memcpy(&x,q,4);v=x;}
      else if(bits==8) v=(int(q[0])-128)/128.0;
      else if(bits==16){int16_t x=int16_t(rd16(q));v=x/32768.0;}
      else if(bits==24){int32_t x=int32_t(q[0]|(q[1]<<8)|(q[2]<<16)); if(x&0x800000)x|=~0xFFFFFF; v=x/8388608.0;}
      else if(bits==32){int32_t x=int32_t(rd32(q));v=x/2147483648.0;}
      else {err=Tr(L"s016");return false;}
      sum+=v;
    }
    out.mono[i]=float(std::clamp(sum/ch,-1.0,1.0));
  } return true;
}

std::vector<float> ResampleLinear(const std::vector<float>& in,double ratio){
  if(in.empty()||ratio<=0)return{}; size_t n=std::max<size_t>(1,size_t(std::llround(in.size()*ratio))); std::vector<float>o(n);
  for(size_t i=0;i<n;i++){double x=i/ratio; size_t a=std::min<size_t>(size_t(x),in.size()-1),b=std::min(a+1,in.size()-1); double t=x-a;o[i]=float(in[a]*(1-t)+in[b]*t);} return o;
}

std::vector<float> TimeStretchOLA(const std::vector<float>& in,double factor){
  // WSOLA-style time stretch.  Unlike a plain overlap/add, this searches
  // around the expected analysis position and aligns the overlapping
  // waveform by correlation.  The local waveform period is therefore kept
  // intact while only the duration changes.
  if(in.empty() || std::abs(factor-1.0)<1e-4) return in;
  factor=std::clamp(factor,0.25,4.0);
  if(in.size()<1024) {
    // Very short one-shot sounds do not give WSOLA enough context.  Pad them
    // temporarily instead of falling back to a resample (which would undo
    // the requested pitch change).
    std::vector<float> padded=in;
    padded.resize(1024,0.0f);
    auto out=TimeStretchOLA(padded,factor);
    const size_t wanted=std::max<size_t>(1,size_t(std::llround(in.size()*factor)));
    out.resize(wanted,0.0f);
    return out;
  }

  constexpr int frame=1024;
  constexpr int overlap=512;
  constexpr int synthHop=frame-overlap; // 512
  constexpr int search=256;
  const double analysisHop=double(synthHop)/factor;
  const size_t wanted=std::max<size_t>(1,size_t(std::llround(in.size()*factor)));
  std::vector<float> out(wanted+frame,0.0f);
  std::vector<float> weight(wanted+frame,0.0f);

  auto win=[frame](int i)->float{
    return 0.5f-0.5f*std::cos(float(2.0*3.14159265358979323846*i/(frame-1)));
  };

  size_t inPos=0, outPos=0;
  int previousBest=0;
  // First frame.
  for(int i=0;i<frame && size_t(i)<in.size() && size_t(i)<out.size();++i){
    const float w=win(i); out[i]+=in[i]*w; weight[i]+=w;
  }

  double predicted=analysisHop;
  outPos=synthHop;
  while(outPos < wanted && predicted < double(in.size())){
    int center=int(std::llround(predicted));
    // Keep the source cursor moving forward.  Feeding the matched position
    // back into the prediction can lock onto an earlier period of a sustained
    // tone, especially for downward pitch shifts, and leave the tail unused.
    int lo=std::max(previousBest+1,center-search);
    int hi=std::min<int>(int(in.size())-frame,center+search);
    if(hi<lo) break;

    int best=lo;
    double bestCorr=-1e300;
    // Compare the candidate's first overlap samples with what is already
    // present at the destination overlap.  Normalized correlation keeps
    // quiet frames from winning purely because of level.
    for(int cand=lo;cand<=hi;++cand){
      double xy=0.0,xx=1e-12,yy=1e-12;
      for(int i=0;i<overlap && outPos+size_t(i)<out.size();++i){
        float existing=weight[outPos+i]>1e-8f ? out[outPos+i]/weight[outPos+i] : 0.0f;
        float src=in[cand+i];
        xy+=double(existing)*src; xx+=double(existing)*existing; yy+=double(src)*src;
      }
      double corr=xy/std::sqrt(xx*yy);
      if(corr>bestCorr){bestCorr=corr;best=cand;}
    }

    inPos=size_t(best);
    for(int i=0;i<frame && inPos+size_t(i)<in.size() && outPos+size_t(i)<out.size();++i){
      const float w=win(i); out[outPos+i]+=in[inPos+i]*w; weight[outPos+i]+=w;
    }
    previousBest=best;
    predicted+=analysisHop;
    outPos+=synthHop;
  }

  out.resize(wanted);
  for(size_t i=0;i<out.size();++i) if(weight[i]>1e-8f) out[i]/=weight[i];
  return out;
}

std::vector<float> ProcessPcm(const std::vector<float>& in,uint32_t inRate,float volume,int transpose,bool pitchShift){
  if(in.empty() || inRate == 0) return {};
  const double pitch = std::pow(2.0, transpose / 12.0);
  const double ratio = 15625.0 / (double(inRate) * pitch);
  auto p = ResampleLinear(in, ratio);

  // Pitch Shift ON: transpose first, then use WSOLA to restore only the
  // duration.  Because WSOLA preserves the local waveform period, it does
  // not cancel the requested pitch transposition.
  if(pitchShift && transpose != 0){
    p = TimeStretchOLA(p, pitch);
    const size_t targetSamples = std::max<size_t>(1,
      size_t(std::llround(double(in.size()) * 15625.0 / double(inRate))));
    if(p.size() > targetSamples) p.resize(targetSamples);
    else if(p.size() < targetSamples) p.resize(targetSamples, 0.0f);
  }

  for(auto& x : p) x = std::clamp(x * volume, -1.0f, 1.0f);
  return p;
}

static const int STEP[49]={16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552};
static const int SHIFT[8]={-1,-1,-1,-1,2,4,6,8};
static int diffFor(int st,int n){int a=STEP[st],m=n&7;int d=a/8;if(m&1)d+=a/4;if(m&2)d+=a/2;if(m&4)d+=a;return(n&8)?-d:d;}
static int clamp12(int x){return std::clamp(x,-2048,2047);}

std::vector<uint8_t> EncodeTarget(const std::vector<float>& p,PcmFormat fmt){
  std::vector<uint8_t>o; if(fmt==PcmFormat::P16){o.reserve(p.size()*2);for(float v:p){int16_t s=(int16_t)std::lround(std::clamp(v,-1.f,1.f)*32767.f);o.push_back(uint8_t((uint16_t(s)>>8)&255));o.push_back(uint8_t(s&255));}return o;}
  if(fmt==PcmFormat::P8){o.reserve(p.size());for(float v:p){int s=std::lround(std::clamp(v,-1.f,1.f)*127.f);o.push_back(uint8_t(int8_t(s)));}return o;}
  o.reserve((p.size()+1)/2); int pred=0,st=0; uint8_t byte=0;
  for(size_t i=0;i<p.size();i++){int target=clamp12(int(std::lround(std::clamp(p[i],-1.f,1.f)*2047.f))); int best=0,be=1<<30,bp=pred,bs=st;
    for(int n=0;n<16;n++){int np=clamp12(pred+diffFor(st,n));int ns=std::clamp(st+SHIFT[n&7],0,48);int e=std::abs(target-np);if(e<be){be=e;best=n;bp=np;bs=ns;}}
    pred=bp;st=bs; if((i&1)==0)byte=uint8_t(best&15);else{o.push_back(uint8_t(byte|((best&15)<<4)));byte=0;}
  } if(p.size()&1)o.push_back(byte); return o;
}

std::vector<float> DecodeTarget(const uint8_t*d,size_t bytes,PcmFormat fmt){
  std::vector<float>o; if(fmt==PcmFormat::P16){o.reserve(bytes/2);for(size_t i=0;i+1<bytes;i+=2){int16_t s=int16_t((d[i]<<8)|d[i+1]);o.push_back(s/32768.f);}return o;}
  if(fmt==PcmFormat::P8){o.reserve(bytes);for(size_t i=0;i<bytes;i++)o.push_back(int8_t(d[i])/128.f);return o;}
  o.reserve(bytes*2);int pred=0,st=0;for(size_t i=0;i<bytes;i++)for(int k=0;k<2;k++){int n=k?((d[i]>>4)&15):(d[i]&15);pred=clamp12(pred+diffFor(st,n));st=std::clamp(st+SHIFT[n&7],0,48);o.push_back(pred/2048.f);}return o;
}

bool WriteMono16Wave(const std::wstring& path,const std::vector<float>& p,uint32_t rate,std::wstring& err){
  std::ofstream f(fs::path(path),std::ios::binary);if(!f){err=Tr(L"s017");return false;}
  auto w16=[&](uint16_t v){uint8_t b[2]={uint8_t(v),uint8_t(v>>8)};f.write((const char*)b,2);};
  auto w32=[&](uint32_t v){uint8_t b[4]={uint8_t(v),uint8_t(v>>8),uint8_t(v>>16),uint8_t(v>>24)};f.write((const char*)b,4);};
  const uint16_t channels=1,bits=16,blockAlign=2;
  const uint32_t data=uint32_t(std::min<uint64_t>(uint64_t(p.size())*2ull,0xFFFFFFFFull));
  const uint32_t riff=36u+data;
  f.write("RIFF",4);w32(riff);f.write("WAVE",4);
  f.write("fmt ",4);w32(16);w16(1);w16(channels);w32(rate);w32(rate*blockAlign);w16(blockAlign);w16(bits);
  f.write("data",4);w32(data);
  size_t samples=data/2;
  for(size_t i=0;i<samples;++i){int16_t sv=(int16_t)std::lround(std::clamp(p[i],-1.f,1.f)*32767.f);w16(uint16_t(sv));}
  if(!f){err=Tr(L"s018");return false;}
  return true;
}

void WaveOutPlayer::Stop(){if(h_){waveOutReset(h_);if(hdr_.dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(h_,&hdr_,sizeof(hdr_));waveOutClose(h_);h_=nullptr;}buf_.clear();ZeroMemory(&hdr_,sizeof(hdr_));}
size_t WaveOutPlayer::PositionSamples() const{
  if(!h_) return 0;
  MMTIME mt{}; mt.wType=TIME_BYTES;
  if(waveOutGetPosition(h_,&mt,sizeof(mt))!=MMSYSERR_NOERROR) return 0;
  if(mt.wType==TIME_BYTES) return size_t(mt.u.cb)/4u; // stereo 16-bit = 4 bytes per sample frame
  if(mt.wType==TIME_SAMPLES) return size_t(mt.u.sample);
  return 0;
}
bool WaveOutPlayer::Play(const std::vector<float>& p,uint32_t rate){
  Stop(); if(p.empty())return false;
  // Explicitly duplicate the mono signal to L/R. Some Windows/audio-device
  // combinations do not center a one-channel waveOut stream as expected.
  buf_.resize(p.size()*2);
  for(size_t i=0;i<p.size();++i){
    int16_t v=(int16_t)std::lround(std::clamp(p[i],-1.f,1.f)*32767);
    buf_[i*2]=v; buf_[i*2+1]=v;
  }
  WAVEFORMATEX w{}; w.wFormatTag=WAVE_FORMAT_PCM; w.nChannels=2;
  w.nSamplesPerSec=rate; w.wBitsPerSample=16; w.nBlockAlign=4; w.nAvgBytesPerSec=rate*4;
  if(waveOutOpen(&h_,WAVE_MAPPER,&w,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){h_=nullptr;return false;}
  hdr_.lpData=(LPSTR)buf_.data(); hdr_.dwBufferLength=DWORD(buf_.size()*sizeof(int16_t));
  waveOutPrepareHeader(h_,&hdr_,sizeof(hdr_));
  return waveOutWrite(h_,&hdr_,sizeof(hdr_))==MMSYSERR_NOERROR;
}
