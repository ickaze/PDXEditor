// SPDX-License-Identifier: 0BSD
// Copyright (C) 2026 by I.C.KaZe

#pragma once
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include <thread>

struct NdsifVoice {
    std::vector<float> samples;
    uint32_t sourceRate = 15625;
    size_t sourceSamples = 0;
    uint64_t start = 0;
    size_t generated = 0;
    std::atomic<size_t> position{0};
    std::atomic<bool> stopped{false};
    std::atomic<bool> finished{false};
};

class NdsifAudio {
public:
    static NdsifAudio& Instance();
    ~NdsifAudio();
    bool Open(int com, std::wstring& error);
    void Close();
    bool Selected() const { return selected_; }
    std::shared_ptr<NdsifVoice> Play(const std::vector<float>& samples, uint32_t rate);
    std::wstring TakeError();
    static std::vector<int> Ports();
private:
    void Run();
    struct Transport;
    std::unique_ptr<Transport> transport_;
    std::thread worker_;
    std::atomic<bool> quit_{false}, selected_{false}, ready_{false};
    std::mutex mutex_;
    std::vector<std::shared_ptr<NdsifVoice>> voices_;
    uint64_t generated_ = 0;
    std::wstring error_;
};
