#pragma once
#include "Common.h"
#include "Audio.h"

struct PdxEntry { uint32_t offset=0,length=0; };
struct ImportedPdx {
    std::vector<uint8_t> bytes;
    int banks=0;
    std::vector<std::array<PdxEntry,96>> table;
    PcmFormat guessed=PcmFormat::ADPCM;
};

bool ReadPdx(const std::wstring& path, ImportedPdx& out, std::wstring& err);
bool ExtractPdxAllBanks(const std::wstring& path, const ImportedPdx& pdx, const std::wstring& folder,
                        std::vector<std::array<std::wstring,96>>& wavPaths, std::wstring& err);
bool WriteExPdxMulti(const std::wstring& path, const std::vector<std::array<std::vector<uint8_t>,96>>& banks,
                     std::wstring& err, std::vector<std::wstring>* log=nullptr);
