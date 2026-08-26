#pragma once
#include "Common.h"
#include "Audio.h"

struct SlotSetting {
    std::wstring wavPath;
    float volume = 1.0f;
    int transpose = 0;
    bool pitchShift = false;
    bool importedRaw = false;
    std::vector<uint8_t> raw;
};

struct BankSetting {
    std::array<SlotSetting, 96> slot{};
};

struct ProjectSetting {
    std::wstring pdxName = L"pcm";
    PcmFormat format = PcmFormat::ADPCM;
    PcmFormat sourceFormat = PcmFormat::ADPCM;
    std::wstring sourcePdx;
    int currentBank = 0;
    int internalOctave = 0;
    int previewVolume = 100; // tool preview only, 0..500 percent
    std::vector<BankSetting> banks{1};

    int windowX = CW_USEDEFAULT;
    int windowY = CW_USEDEFAULT;
    int windowW = 1280;
    int windowH = 820;
    std::array<int, 6> listWidths{220, 145, 90, 75, 90, 110};
};

bool SaveProject(const std::wstring& path, const ProjectSetting& s, std::wstring& err);
bool LoadProject(const std::wstring& path, ProjectSetting& s, std::wstring& err);
