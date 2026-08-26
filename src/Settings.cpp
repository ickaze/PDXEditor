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

static void ensureBank(ProjectSetting& s, int b) {
    if (b < 0) return;
    if ((int)s.banks.size() <= b) s.banks.resize((size_t)b + 1);
}

bool SaveProject(const std::wstring& path, const ProjectSetting& s, std::wstring& err) {
    std::ofstream f(fs::path(path), std::ios::binary);
    if (!f) { err = L"設定ファイルを書き込めません"; return false; }

    f << "# PDXEditor editable project v2\n"
      << "pdxName=" << esc(s.pdxName) << "\n"
      << "format=" << (int)s.format << "\n"
      << "sourceFormat=" << (int)s.sourceFormat << "\n"
      << "sourcePdx=" << esc(s.sourcePdx) << "\n"
      << "currentBank=" << s.currentBank << "\n"
      << "internalOctave=" << s.internalOctave << "\n"
      << "previewVolume=" << s.previewVolume << "\n"
      << "bankCount=256\n"
      << "windowX=" << s.windowX << "\nwindowY=" << s.windowY
      << "\nwindowW=" << s.windowW << "\nwindowH=" << s.windowH << "\n";
    for (int i = 0; i < 6; ++i) f << "listWidth." << i << "=" << s.listWidths[i] << "\n";

    for (size_t b = 0; b < s.banks.size() && b < 256; ++b) {
        for (int i = 0; i < 96; ++i) {
            const auto& a = s.banks[b].slot[i];
            if (a.wavPath.empty() && a.volume == 1.0f && a.transpose == 0 && !a.pitchShift && !a.importedRaw) continue;
            f << "bank." << b << ".slot." << i << ".path=" << esc(a.wavPath) << "\n"
              << "bank." << b << ".slot." << i << ".volume=" << a.volume << "\n"
              << "bank." << b << ".slot." << i << ".transpose=" << a.transpose << "\n"
              << "bank." << b << ".slot." << i << ".pitchShift=" << (a.pitchShift ? 1 : 0) << "\n"
              << "bank." << b << ".slot." << i << ".importedRaw=" << (a.importedRaw ? 1 : 0) << "\n";
        }
    }
    return true;
}

bool LoadProject(const std::wstring& path, ProjectSetting& s, std::wstring& err) {
    std::ifstream f(fs::path(path), std::ios::binary);
    if (!f) { err = L"設定ファイルを開けません"; return false; }
    ProjectSetting n;
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
            else if (k == "bankCount") { /* v2 compatibility: UI always has 256 banks */ }
            else if (k == "windowX") n.windowX = std::stoi(v);
            else if (k == "windowY") n.windowY = std::stoi(v);
            else if (k == "windowW") n.windowW = std::max(640, std::stoi(v));
            else if (k == "windowH") n.windowH = std::max(480, std::stoi(v));
            else if (k.rfind("listWidth.", 0) == 0) {
                int c = std::stoi(k.substr(10));
                if (c >= 0 && c < 6) n.listWidths[c] = std::clamp(std::stoi(v), 30, 2000);
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
                else if (fld == "importedRaw") a.importedRaw = std::stoi(v) != 0;
            }
        } catch (...) {}
    }
    n.currentBank = std::clamp(n.currentBank, 0, 255);
    if ((int)n.banks.size() <= n.currentBank) n.banks.resize((size_t)n.currentBank + 1);
    s = std::move(n);
    return true;
}
