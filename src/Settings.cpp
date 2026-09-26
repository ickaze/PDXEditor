#include "Settings.h"

static std::string esc(const std::wstring& w) {
    std::string s = WToUtf8(w), o;
    for (char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c != '\r') o += c;
    }
    return o;
}

static std::wstring unesc(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char c = s[++i];
            o += (c == 'n' ? '\n' : c);
        } else o += s[i];
    }
    return Utf8ToW(o);
}


static std::wstring pathForSave(const std::wstring& value, const fs::path& base) {
    if (value.empty()) return {};
    try {
        fs::path ap = fs::absolute(fs::path(value)).lexically_normal();
        fs::path bp = fs::absolute(base).lexically_normal();
        fs::path rel = ap.lexically_relative(bp);
        if (!rel.empty()) {
            auto it = rel.begin();
            if (it == rel.end() || *it != L"..") return rel.wstring();
        }
        return ap.wstring();
    } catch (...) { return value; }
}

static std::wstring pathForLoad(const std::wstring& value, const fs::path& base) {
    if (value.empty()) return {};
    try {
        fs::path p(value);
        if (p.is_relative()) p = base / p;
        return fs::absolute(p).lexically_normal().wstring();
    } catch (...) { return value; }
}

static void ensureBank(ProjectSetting& s, int b) {
    if (b < 0) return;
    if ((int)s.banks.size() <= b) s.banks.resize((size_t)b + 1);
}

bool SaveProject(const std::wstring& path, const ProjectSetting& s, std::wstring& err) {
    fs::path projectFile(path);
    fs::path base = projectFile.has_parent_path() ? fs::absolute(projectFile).parent_path() : fs::current_path();
    std::ofstream f(projectFile, std::ios::binary);
    if (!f) { err = Tr(L"s019"); return false; }

    f << "# PDXEditor editable project v2\n"
      << "pdxName=" << esc(s.pdxName) << "\n"
      << "format=" << (int)s.format << "\n"
      << "sourceFormat=" << (int)s.sourceFormat << "\n"
      << "sourcePdx=" << esc(pathForSave(s.sourcePdx, base)) << "\n"
      << "currentBank=" << s.currentBank << "\n"
      << "internalOctave=" << s.internalOctave << "\n"
      << "previewVolume=" << s.previewVolume << "\n"
      << "language=" << s.language << "\n"
      << "languageCode=" << esc(s.languageCode) << "\n"
      << "bankCount=256\n"
      << "windowX=" << s.windowX << "\nwindowY=" << s.windowY
      << "\nwindowW=" << s.windowW << "\nwindowH=" << s.windowH << "\n"
      << "listColumnCount=9\n"
      << "rightListPercent=" << s.rightListPercent << "\n";
    for (int i = 0; i < 9; ++i) f << "listWidth." << i << "=" << s.listWidths[i] << "\n";

    for (size_t b = 0; b < s.banks.size() && b < 256; ++b) {
        for (int i = 0; i < 96; ++i) {
            const auto& a = s.banks[b].slot[i];
            if (a.wavPath.empty() && a.volume == 1.0f && a.transpose == 0 && !a.pitchShift && a.trimStart==0 && a.trimEnd==0 && !a.importedRaw) continue;
            f << "bank." << b << ".slot." << i << ".path=" << esc(pathForSave(a.wavPath, base)) << "\n"
              << "bank." << b << ".slot." << i << ".volume=" << a.volume << "\n"
              << "bank." << b << ".slot." << i << ".transpose=" << a.transpose << "\n"
              << "bank." << b << ".slot." << i << ".pitchShift=" << (a.pitchShift ? 1 : 0) << "\n"
              << "bank." << b << ".slot." << i << ".trimStart=" << a.trimStart << "\n"
              << "bank." << b << ".slot." << i << ".trimEnd=" << a.trimEnd << "\n"
              << "bank." << b << ".slot." << i << ".importedRaw=" << (a.importedRaw ? 1 : 0) << "\n";
        }
    }
    return true;
}

bool LoadProject(const std::wstring& path, ProjectSetting& s, std::wstring& err) {
    fs::path projectFile(path);
    fs::path base = projectFile.has_parent_path() ? fs::absolute(projectFile).parent_path() : fs::current_path();
    std::ifstream f(projectFile, std::ios::binary);
    if (!f) { err = Tr(L"s020"); return false; }
    ProjectSetting n;
    std::array<int, 6> legacy6{220,145,90,75,90,110};
    std::array<int, 7> legacy7{260,220,145,90,75,90,110};
    int listColumnCount = 0;
    n.banks.clear();
    n.banks.resize(1);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto p = line.find('=');
        if (p == std::string::npos) continue;
        auto k = line.substr(0, p), v = line.substr(p + 1);
        try {
            if (k == "pdxName") n.pdxName = unesc(v);
            else if (k == "format") n.format = (PcmFormat)std::clamp(std::stoi(v), 0, 2);
            else if (k == "sourceFormat") n.sourceFormat = (PcmFormat)std::clamp(std::stoi(v), 0, 2);
            else if (k == "sourcePdx") n.sourcePdx = unesc(v);
            else if (k == "currentBank") n.currentBank = std::max(0, std::stoi(v));
            else if (k == "internalOctave") n.internalOctave = std::clamp(std::stoi(v), 0, 8);
            else if (k == "previewVolume") n.previewVolume = std::clamp(std::stoi(v), 0, 500);
            else if (k == "language") n.language = std::max(-1, std::stoi(v));
            else if (k == "languageCode") n.languageCode = unesc(v);
            else if (k == "bankCount") { /* v2 compatibility: UI always has 256 banks */ }
            else if (k == "windowX") n.windowX = std::stoi(v);
            else if (k == "windowY") n.windowY = std::stoi(v);
            else if (k == "windowW") n.windowW = std::max(640, std::stoi(v));
            else if (k == "windowH") n.windowH = std::max(480, std::stoi(v));
            else if (k == "listColumnCount") listColumnCount = std::max(0,std::stoi(v));
            else if (k == "rightListPercent") n.rightListPercent = std::clamp(std::stoi(v), 20, 85);
            else if (k.rfind("listWidth.", 0) == 0) {
                int c = std::stoi(k.substr(10));
                int width = std::clamp(std::stoi(v), 30, 2000);
                if (listColumnCount >= 9) {
                    if (c >= 0 && c < 9) n.listWidths[c] = width;
                } else if (listColumnCount >= 7) {
                    if (c >= 0 && c < 7) legacy7[c] = width;
                } else if (c >= 0 && c < 6) {
                    legacy6[c] = width;
                }
            }
            else if (k.rfind("bank.", 0) == 0) {
                auto p1 = k.find('.', 5); if (p1 == std::string::npos) continue;
                int b = std::stoi(k.substr(5, p1 - 5));
                auto tag = k.substr(p1 + 1);
                if (tag.rfind("slot.", 0) != 0) continue;
                auto p2 = tag.find('.', 5); if (p2 == std::string::npos) continue;
                int i = std::stoi(tag.substr(5, p2 - 5));
                if (b < 0 || b >= 256 || i < 0 || i >= 96) continue;
                ensureBank(n, b);
                auto& a = n.banks[b].slot[i];
                auto fld = tag.substr(p2 + 1);
                if (fld == "path") a.wavPath = unesc(v);
                else if (fld == "volume") a.volume = std::clamp(std::stof(v), 0.f, 2.f);
                else if (fld == "transpose") a.transpose = std::clamp(std::stoi(v), -24, 24);
                else if (fld == "pitchShift" || fld == "timeStretch") a.pitchShift = std::stoi(v) != 0;
                else if (fld == "trimStart") a.trimStart = std::stoull(v);
                else if (fld == "trimEnd") a.trimEnd = std::stoull(v);
                else if (fld == "importedRaw") a.importedRaw = std::stoi(v) != 0;
            }
            // v1 backward compatibility
            else if (k.rfind("slot.", 0) == 0) {
                auto q = k.find('.', 5); if (q == std::string::npos) continue;
                int i = std::stoi(k.substr(5, q - 5)); if (i < 0 || i >= 96) continue;
                auto& a = n.banks[0].slot[i]; auto fld = k.substr(q + 1);
                if (fld == "path") a.wavPath = unesc(v);
                else if (fld == "volume") a.volume = std::clamp(std::stof(v), 0.f, 2.f);
                else if (fld == "transpose") a.transpose = std::clamp(std::stoi(v), -24, 24);
                else if (fld == "pitchShift" || fld == "timeStretch") a.pitchShift = std::stoi(v) != 0;
                else if (fld == "trimStart") a.trimStart = std::stoull(v);
                else if (fld == "trimEnd") a.trimEnd = std::stoull(v);
                else if (fld == "importedRaw") a.importedRaw = std::stoi(v) != 0;
            }
        } catch (...) {}
    }
    if (listColumnCount < 9) {
        if (listColumnCount >= 7) {
            // fixed18-fixed26: Path, File, Timestamp, Rate, Bits, Channels, Format.
            n.listWidths = {legacy7[0],legacy7[1],legacy7[2],95,90,legacy7[3],legacy7[4],legacy7[5],legacy7[6]};
        } else {
            // Older projects: File, Timestamp, Rate, Bits, Channels, Format.
            n.listWidths = {260,legacy6[0],legacy6[1],95,90,legacy6[2],legacy6[3],legacy6[4],legacy6[5]};
        }
    }
    n.currentBank = std::clamp(n.currentBank, 0, 255);
    if ((int)n.banks.size() <= n.currentBank) n.banks.resize((size_t)n.currentBank + 1);
    n.sourcePdx = pathForLoad(n.sourcePdx, base);
    for (auto& bank : n.banks) for (auto& slot : bank.slot)
        slot.wavPath = pathForLoad(slot.wavPath, base);
    s = std::move(n);
    return true;
}
