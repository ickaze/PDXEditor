#pragma once
#include "Common.h"
#include "Audio.h"
#include "Localization.h"

struct SlotSetting {
    std::wstring wavPath;
    float volume = 1.0f;
    int transpose = 0;
    bool pitchShift = false;
    uint64_t trimStart = 0; // source WAV sample index
    uint64_t trimEnd = 0;   // exclusive; 0 means end of WAV
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
    int language = -1; // legacy index: 0=Japanese, 1=English
    std::wstring languageCode; // external language pack code, e.g. ja/en
    std::vector<BankSetting> banks{1};

    int windowX = CW_USEDEFAULT;
    int windowY = CW_USEDEFAULT;
    int windowW = 1280;
    int windowH = 820;
    std::array<int, 9> listWidths{260, 220, 145, 95, 90, 90, 75, 90, 110};
    int rightListPercent = 70; // vertical split: WAV list vs log
};

bool SaveProject(const std::wstring& path, const ProjectSetting& s, std::wstring& err);
bool LoadProject(const std::wstring& path, ProjectSetting& s, std::wstring& err);
