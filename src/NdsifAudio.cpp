// SPDX-License-Identifier: 0BSD
// Copyright (C) 2026 by I.C.KaZe

#include "NdsifAudio.h"
#include "NdsifWire.h"

namespace nd = mdx::nd;
static std::wstring FailureText(const char* message) {
    const std::string text(message);
    return std::wstring(text.begin(), text.end());
}
struct NdsifAudio::Transport {
    HANDLE port = INVALID_HANDLE_VALUE;
    uint16_t sequence = 0;
    uint32_t accepted = 0, played = 0, capacity = 0;
    size_t payload = 256;
    nd::Encoder encoder;
    bool started = false;
    ~Transport() {
        if (port != INVALID_HANDLE_VALUE) {
            try { Ask(0); } catch (...) {}
            CloseHandle(port);
        }
    }
    void Send(int type, int command, const nd::B& data = {}) {
        auto packet = nd::frame(type, command, ++sequence, data);
        DWORD written = 0;
        if (!WriteFile(port, packet.data(), DWORD(packet.size()), &written, nullptr) || written != packet.size())
            throw std::runtime_error("NDSIF: serial write failed");
    }
    nd::B Ask(int command, const nd::B& data = {}) {
        Send(1, command, data);
        nd::B encoded, raw;
        bool overflow = false;
        auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(700);
        while (std::chrono::steady_clock::now() < until) {
            uint8_t bytes[512]; DWORD count = 0;
            if (!ReadFile(port, bytes, sizeof(bytes), &count, nullptr))
                throw std::runtime_error("NDSIF: serial read failed");
            for (DWORD i = 0; i < count; ++i) {
                if (bytes[i]) {
                    if (encoded.size() < 269) encoded.push_back(bytes[i]);
                    else overflow = true;
                    continue;
                }
                if (!overflow && nd::decode(encoded, raw) && raw[3] == 2 && raw[4] == command && nd::r16(raw, 5) == sequence) {
                    if (raw.size() < 12 || raw[9]) throw std::runtime_error("NDSIF: control rejected (" + std::to_string(command) + ")");
                    return nd::B(raw.begin() + 9, raw.end() - 2);
                }
                encoded.clear(); overflow = false;
            }
        }
        throw std::runtime_error("NDSIF: response timeout (" + std::to_string(command) + ")");
    }
    void Open(int com) {
        const auto name = L"\\\\.\\COM" + std::to_wstring(com);
        port = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (port == INVALID_HANDLE_VALUE) throw std::runtime_error("NDSIF: cannot open COM port");
        DCB d{}; d.DCBlength = sizeof(d);
        if (!GetCommState(port, &d)) throw std::runtime_error("NDSIF: COM configuration failed");
        d.BaudRate = 115200; d.ByteSize = 8; d.Parity = NOPARITY; d.StopBits = ONESTOPBIT;
        d.fBinary = 1; d.fParity = d.fOutX = d.fInX = d.fOutxCtsFlow = d.fOutxDsrFlow = d.fDsrSensitivity = 0;
        d.fDtrControl = DTR_CONTROL_ENABLE; d.fRtsControl = RTS_CONTROL_DISABLE;
        COMMTIMEOUTS t{MAXDWORD, MAXDWORD, 10, 0, 300};
        if (!SetCommState(port, &d) || !SetCommTimeouts(port, &t)) throw std::runtime_error("NDSIF: COM configuration failed");
        PurgeComm(port, PURGE_RXCLEAR | PURGE_TXCLEAR);
        if (Ask(1, {0x50, 0x44}) != nd::B({0, 0x50, 0x44})) throw std::runtime_error("NDSIF: PING mismatch");
        Ask(2);
        unsigned offset = 0;
        std::set<unsigned> offsets, entries;
        bool stream = false, variable = false;
        uint32_t fixed = 0;
        do {
            if (!offsets.insert(offset).second) throw std::runtime_error("NDSIF: capability page cycle");
            nd::B p; nd::u16(p, uint16_t(offset)); auto b = Ask(5, p);
            if (b.size() < 14 || b.size() != size_t(14 + b[13] * 9)) throw std::runtime_error("NDSIF: invalid capabilities");
            const auto nextPayload = nd::r16(b, 1); const auto nextCapacity = nd::r32(b, 3);
            if (offsets.size() > 1 && (payload != nextPayload || capacity != nextCapacity)) throw std::runtime_error("NDSIF: inconsistent capabilities");
            payload = nextPayload; capacity = nextCapacity; offset = nd::r16(b, 11);
            for (size_t i = 14; i < b.size(); i += 9) {
                if (!entries.insert(unsigned(b[i]) | (unsigned(b[i+1]) << 8) | (unsigned(b[i+2]) << 16)).second)
                    throw std::runtime_error("NDSIF: duplicate capability");
                if (b[i] != 0xb7 || b[i+1] || b[i+2]) continue;
                const int mode = b[i+4]; const bool clockFlag = (b[i+3] & 4) != 0;
                fixed = nd::r32(b, i+5);
                if (mode > 2 || (mode == 1 ? (!fixed || clockFlag) : (fixed || clockFlag != (mode == 2))))
                    throw std::runtime_error("NDSIF: invalid clock capability");
                stream = (b[i+3] & 8) != 0; variable = mode == 2;
            }
        } while (offset != 65535);
        if (!stream || payload < 7 || payload > 256 || capacity < 256)
            throw std::runtime_error("NDSIF: MSM6258 ADPCM stream unavailable");
        if (!variable && fixed != 8000000) throw std::runtime_error("NDSIF: 8 MHz MSM6258 clock required");
        Ask(0);
        if (variable) { nd::B p{0xb7, 0}; nd::u32(p, 8000000); Ask(4, p); }
        auto b = Status();
        for (size_t i = 1; i < b.size(); ++i) if (b[i]) throw std::runtime_error("NDSIF: stream reset failed");
    }
    nd::B Status() {
        auto b = Ask(0x13, {0xb7, 0});
        if (b.size() != 41 || (nd::r32(b, 29) & 4)) throw std::runtime_error("NDSIF: ADPCM stream fault");
        if (nd::r32(b, 1) != accepted || nd::r32(b, 5) > accepted || nd::r32(b, 9) != accepted - nd::r32(b, 5))
            throw std::runtime_error("NDSIF: invalid stream position");
        played = nd::r32(b, 5);
        return b;
    }
    void Data(const nd::B& bytes) {
        for (size_t i = 0; i < bytes.size();) {
            size_t count = std::min({size_t(250), payload - 6, bytes.size() - i});
            if (count > UINT32_MAX - accepted) throw std::runtime_error("NDSIF: stream position exhausted; reconnect output");
            nd::B p{0xb7, 0}; nd::u32(p, accepted);
            p.insert(p.end(), bytes.begin() + i, bytes.begin() + i + count);
            Send(0, 0x10, p); accepted += uint32_t(count); i += count;
        }
    }
    void Start() {
        Ask(1);
        nd::B p{0xb7, 0}; nd::u32(p, 8000000); nd::u16(p, 512);
        Send(0, 0x12, p); started = true;
    }
};

NdsifAudio& NdsifAudio::Instance() { static NdsifAudio audio; return audio; }
NdsifAudio::~NdsifAudio() { Close(); }
std::vector<int> NdsifAudio::Ports() {
    // Enumerate DOS device names without opening or resetting unrelated ports.
    std::vector<wchar_t> names(65536);
    DWORD n = QueryDosDeviceW(nullptr, names.data(), DWORD(names.size()));
    std::vector<int> result;
    for (const wchar_t* p = names.data(); n && *p; p += wcslen(p) + 1) {
        if (wcsncmp(p, L"COM", 3) || !p[3]) continue;
        wchar_t* end = nullptr; long com = wcstol(p + 3, &end, 10);
        if (*end == 0 && com > 0 && com <= 65535) result.push_back(int(com));
    }
    std::sort(result.begin(), result.end());
    return result;
}
bool NdsifAudio::Open(int com, std::wstring& error) {
    Close(); selected_ = true;
    try {
        transport_ = std::make_unique<Transport>(); transport_->Open(com);
        quit_ = false; ready_ = true;
        worker_ = std::thread([this] { Run(); });
        return true;
    } catch (const std::exception& e) {
        error = FailureText(e.what()); Close(); return false;
    }
}
void NdsifAudio::Close() {
    quit_ = true; ready_ = false;
    if (worker_.joinable()) worker_.join();
    transport_.reset();
    std::lock_guard<std::mutex> guard(mutex_);
    for (auto& v : voices_) v->stopped = true;
    voices_.clear(); generated_ = 0; selected_ = false; error_.clear();
}
std::shared_ptr<NdsifVoice> NdsifAudio::Play(const std::vector<float>& samples, uint32_t rate) {
    if (!ready_ || samples.empty() || !rate) return {};
    auto v = std::make_shared<NdsifVoice>(); v->sourceRate = rate; v->sourceSamples = samples.size();
    const size_t count = std::max<size_t>(1, size_t(std::llround(samples.size() * 15625.0 / rate)));
    v->samples.resize(count);
    for (size_t i = 0; i < count; ++i) {
        double x = i * double(rate) / 15625.0;
        size_t a = std::min(size_t(x), samples.size() - 1), b = std::min(a + 1, samples.size() - 1);
        v->samples[i] = float(samples[a] * (1 - (x - a)) + samples[b] * (x - a));
    }
    std::lock_guard<std::mutex> guard(mutex_);
    if (!ready_) return {};
    v->start = generated_; voices_.push_back(v); return v;
}
std::wstring NdsifAudio::TakeError() {
    std::lock_guard<std::mutex> guard(mutex_);
    std::wstring result; result.swap(error_); return result;
}
void NdsifAudio::Run() {
    try {
        auto& t = *transport_;
        uint32_t previous = 0;
        auto progress = std::chrono::steady_clock::now();
        const uint32_t ahead = std::min<uint32_t>(t.capacity - 128, 384);
        while (!quit_) {
            if (t.started) {
                t.Status();
                if (t.played != previous) { previous = t.played; progress = std::chrono::steady_clock::now(); }
                if (std::chrono::steady_clock::now() - progress > std::chrono::seconds(2)) throw std::runtime_error("NDSIF: playback stalled");
            }
            bool idle = false;
            {
                std::lock_guard<std::mutex> guard(mutex_);
                const uint64_t audible = uint64_t(t.played) * 2;
                for (auto& v : voices_) {
                    const uint64_t pos = audible > v->start ? audible - v->start : 0;
                    v->position = pos >= v->samples.size() ? v->sourceSamples : std::min(v->sourceSamples, size_t(pos * v->sourceRate / 15625));
                    if (pos >= v->samples.size()) v->finished = true;
                }
                voices_.erase(std::remove_if(voices_.begin(), voices_.end(), [](const auto& v) { return v->stopped || v->finished; }), voices_.end());
                idle = voices_.empty();
            }
            if ((!t.started && idle) || t.accepted - t.played >= ahead) { Sleep(2); continue; }
            nd::B bytes;
            const size_t frames = size_t(std::min<uint32_t>(64, ahead - (t.accepted - t.played))) * 2;
            {
                std::lock_guard<std::mutex> guard(mutex_);
                for (size_t i = 0; i < frames; ++i) {
                    double mix = 0;
                    for (auto& v : voices_) if (!v->stopped && v->generated < v->samples.size()) mix += v->samples[v->generated++];
                    t.encoder.sample(int(std::lround(std::clamp(mix, -1.0, 1.0) * 2047)), bytes);
                }
                generated_ += frames;
            }
            t.Data(bytes);
            if (!t.started && t.accepted >= ahead) { t.Start(); progress = std::chrono::steady_clock::now(); }
        }
    } catch (const std::exception& e) {
        ready_ = false;
        std::lock_guard<std::mutex> guard(mutex_);
        error_ = FailureText(e.what());
        for (auto& v : voices_) v->stopped = true;
    }
}
