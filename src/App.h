#pragma once
#include "Common.h"
#include "Audio.h"
#include "Pdx.h"
#include "Settings.h"

class App {
public:
    static App* self;
    int Run(HINSTANCE h, const std::wstring& startupProject = L"");
private:
    struct HistoryState { ProjectSetting prj; std::vector<std::wstring> files; };

    HINSTANCE inst_{};
    HWND hwnd_{}, keys_{}, list_{}, log_{}, name_{}, fmt_{}, bankCombo_{}, midiCombo_{}, midiToggle_{};
    HWND volTrack_{}, volEdit_{}, trTrack_{}, trEdit_{}, stretch_{}, slotLabel_{};
    HWND previewTrack_{}, previewLabel_{}, waveEdit_{}, waveScroll_{};
    HWND languageCombo_{}, languageLabel_{}, pdxNameLabel_{}, formatLabel_{}, loadBtn_{}, convertBtn_{}, bankLabel_{}, volumeLabel_{}, transposeLabel_{};
    HFONT font_{};
    ProjectSetting prj_;
    std::wstring projectPath_;
    std::wstring startupProject_;
    std::vector<std::wstring> files_;
    std::map<std::wstring,WaveData> cache_;
    WaveOutPlayer mousePlayer_;
    std::array<WaveOutPlayer,96> midiPlayers_;
    std::array<WaveOutPlayer,12> keyPlayers_;
    std::vector<float> listDisplayWave_, mouseKeyDisplayWave_;
    uint32_t listDisplayRate_=15625;
    std::array<std::vector<float>,96> midiDisplayWave_;
    std::array<std::vector<float>,12> keyDisplayWave_;
    std::set<int> midiDown_, keyDownSemis_;
    std::array<float,96> midiPressure_{};
    float channelPressure_=1.0f;
    int selected_=0;
    int dragFile_=-1;
    bool listHolding_=false, keyHolding_=false, listEnterPreview_=false, keyEnterPreview_=false, dirty_=false, updating_=false, restoringHistory_=false;
    bool splitterDrag_=false; int splitterY_=0;
    enum class WaveDragMode { None, Range, Start, End };
    WaveDragMode waveDrag_=WaveDragMode::None;
    uint64_t waveDragAnchor_=0; double waveViewStart_=0.0, waveViewEnd_=1.0;
    std::optional<HistoryState> pendingWaveEdit_;
    uint64_t changedAt_=0;
    HMIDIIN midiIn_=nullptr;
    std::vector<HistoryState> undo_, redo_;
    std::optional<HistoryState> pendingControlEdit_;

    static LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK KeysProc(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK ListProc(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK WaveProc(HWND,UINT,WPARAM,LPARAM);
    static void CALLBACK MidiCb(HMIDIIN,UINT,DWORD_PTR,DWORD_PTR,DWORD_PTR);

    LRESULT OnMsg(UINT,WPARAM,LPARAM);
    LRESULT OnKeys(HWND,UINT,WPARAM,LPARAM);
    LRESULT OnList(HWND,UINT,WPARAM,LPARAM);
    LRESULT OnWave(HWND,UINT,WPARAM,LPARAM);
    void CreateUI(); void Layout(); void ApplyLanguage(); void PaintKeys(HDC); int HitKey(POINT) const;
    void PaintWaveEditor(HDC); void ResetWaveView(); void UpdateWaveScroll(); void ScrollWaveTo(int pos); uint64_t WaveSampleAtX(int x) const; int WaveXForSample(uint64_t sample) const; void CommitWaveEdit(); void FindZeroCross(bool startEdge,bool outward,bool recordUndo=true);
    void ReloadLanguageCombo(); void DrawWaveOverlay(HDC,const RECT&,const std::vector<float>&); void DrawPlaybackBar(HDC,const RECT&,double); std::vector<float> BuildKeyDisplayMix() const; std::vector<float> BuildScrollingWindow(const std::vector<float>&,size_t,uint32_t) const;
    void SelectKey(int,bool play=false,float velocity=1.f); void PreviewKey(int,float velocity=1.f); void StopPreview();
    std::vector<float> ApplyPreviewGain(const std::vector<float>& in) const;
    std::vector<float> TrimForSlot(const WaveData& w,const SlotSetting& s) const;
    void AddDropped(HDROP); void AddWavePath(const std::wstring&); void AddFolder(const std::wstring&); void AddToList(const std::wstring&,const WaveData&); void SortList(int col); void RebuildFileList();
    void Log(const std::wstring&); void MarkDirty(); void SaveAuto(); bool SaveCurrentNow();
    void LoadProjectFile(const std::wstring&,bool ask); void ImportPdx(const std::wstring&); void Convert();
    void UpdateControls(); void ReadControls(); void RefreshBankCombo(); void SwitchBank(int bank,bool fromProgramChange=false);
    std::wstring CurrentProjectPath() const; bool GetWave(const std::wstring&,WaveData&); void CaptureUiState(); void ApplyUiState();
    void RefreshMidi(); void OpenMidi(); void CloseMidi(); void HandleMidi(DWORD msg);
    void KeyboardNote(int sem,bool down); bool FocusIsText() const;
    void AssignSelected(const std::wstring& path); void DeleteFocused(); void FinishListGesture();
    HistoryState Snapshot() const; void PushUndo(); void Undo(); void Redo(); void RestoreState(const HistoryState&);
    void BeginControlEdit(); void EndControlEdit();
    SlotSetting& CurSlot(); const SlotSetting& CurSlot() const;
};
