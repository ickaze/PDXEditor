#include "App.h"
#include <windowsx.h>
#include "Localization.h"

App* App::self=nullptr;
static WNDPROC gOldList=nullptr;
static const wchar_t* kClass=L"PDXEditorMain";
static const wchar_t* kKeys=L"PDXEditorKeys";
static const wchar_t* kWave=L"PDXEditorWaveEdit";

static fs::path AppDirectory(){
    wchar_t ex[32768]{};
    DWORD n=GetModuleFileNameW(nullptr,ex,32768);
    if(n==0||n>=32768) return fs::current_path();
    return fs::path(ex).parent_path();
}
static fs::path LastProjectMarkerPath(){ return AppDirectory()/L"PDXEditor.last"; }

static RECT ListBodyRect(HWND h){
    RECT rc{}; GetClientRect(h,&rc);
    HWND header=ListView_GetHeader(h);
    if(header){
        RECT hr{}; GetWindowRect(header,&hr);
        POINT pt{hr.left,hr.bottom}; ScreenToClient(h,&pt);
        rc.top=std::max((int)rc.top,(int)pt.y);
    }
    return rc;
}
static void InvalidateListBody(HWND h){
    if(!h)return;
    RECT rc=ListBodyRect(h);
    InvalidateRect(h,&rc,FALSE);
}

enum {
    ID_NAME=100, ID_FMT, ID_LOAD, ID_CONVERT, ID_BANK,
    ID_MIDICOMBO, ID_MIDITOGGLE, ID_LIST, ID_LOG,
    ID_VOLTR, ID_VOLEDIT, ID_TRTR, ID_TREDIT, ID_STRETCH, ID_PREVIEWTR, ID_LANGUAGE, ID_WAVESCROLL
};

static int PitchToPdx(int octave,int semi){ return octave*12+semi-3; }
static bool ValidPdx(int idx){ return idx>=0 && idx<96; }
static bool IsBlackSemi(int s){ return s==1||s==3||s==6||s==8||s==10; }

static std::vector<wchar_t> MakeOpenFilter(){
    std::vector<wchar_t> out;
    auto add=[&](const wchar_t* x){size_t n=wcslen(x);out.insert(out.end(),x,x+n);out.push_back(L'\0');};
    add(Tr(L"s021")); add(L"*.pdxedit;*.pdx");
    add(Tr(L"s022")); add(L"*.pdxedit");
    add(Tr(L"s023")); add(L"*.pdx");
    add(Tr(L"s024")); add(L"*.*"); out.push_back(L'\0'); return out;
}


int App::Run(HINSTANCE h, const std::wstring& startupProject){
    self=this; inst_=h; startupProject_=startupProject;
    LoadLanguagePacks(AppDirectory());
    int autoLang=LanguageIndexByCode(IsWindowsUiJapanese()?L"ja":L"en"); if(autoLang<0)autoLang=0;
    prj_.language=autoLang; prj_.languageCode=gLanguagePacks[autoLang].code; SelectLanguageIndex(autoLang);
    INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_LISTVIEW_CLASSES|ICC_BAR_CLASSES}; InitCommonControlsEx(&ic);
    WNDCLASSW wc{}; wc.hInstance=h; wc.lpfnWndProc=WndProc; wc.lpszClassName=kClass; wc.hCursor=LoadCursor(nullptr,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); RegisterClassW(&wc);
    WNDCLASSW kc=wc; kc.lpfnWndProc=KeysProc; kc.lpszClassName=kKeys; kc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH); RegisterClassW(&kc);
    WNDCLASSW wvc=wc; wvc.lpfnWndProc=WaveProc; wvc.lpszClassName=kWave; wvc.hCursor=LoadCursor(nullptr,IDC_CROSS); wvc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); RegisterClassW(&wvc);
    hwnd_=CreateWindowExW(0,kClass,L"MXDRV EX-PDX PCM Editor",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1280,860,nullptr,nullptr,h,nullptr);
    ShowWindow(hwnd_,SW_SHOW); UpdateWindow(hwnd_);
    MSG m{};
    while(GetMessage(&m,nullptr,0,0)>0){
        if((m.message==WM_KEYDOWN||m.message==WM_KEYUP)&&!FocusIsText()){ OnMsg(m.message,m.wParam,m.lParam); continue; }
        TranslateMessage(&m); DispatchMessage(&m);
    }
    return (int)m.wParam;
}

LRESULT CALLBACK App::WndProc(HWND h,UINT m,WPARAM w,LPARAM l){ if(self){ if(!self->hwnd_)self->hwnd_=h; return self->OnMsg(m,w,l);} return DefWindowProc(h,m,w,l); }
LRESULT CALLBACK App::KeysProc(HWND h,UINT m,WPARAM w,LPARAM l){ return self?self->OnKeys(h,m,w,l):DefWindowProc(h,m,w,l); }
LRESULT CALLBACK App::ListProc(HWND h,UINT m,WPARAM w,LPARAM l){ return self?self->OnList(h,m,w,l):CallWindowProc(gOldList,h,m,w,l); }
LRESULT CALLBACK App::WaveProc(HWND h,UINT m,WPARAM w,LPARAM l){ return self?self->OnWave(h,m,w,l):DefWindowProc(h,m,w,l); }
void CALLBACK App::MidiCb(HMIDIIN,UINT msg,DWORD_PTR,DWORD_PTR p1,DWORD_PTR){ if(self&&msg==MIM_DATA)PostMessage(self->hwnd_,WM_APP+1,(WPARAM)p1,0); }

SlotSetting& App::CurSlot(){ return prj_.banks[prj_.currentBank].slot[selected_]; }
const SlotSetting& App::CurSlot() const { return prj_.banks[prj_.currentBank].slot[selected_]; }

LRESULT App::OnMsg(UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE:
        CreateUI(); DragAcceptFiles(hwnd_,TRUE); SetTimer(hwnd_,1,250,nullptr); SetTimer(hwnd_,2,16,nullptr); return 0;
    case WM_MOVE:
        if(!updating_){CaptureUiState(); MarkDirty();} return 0;
    case WM_SIZE:
        Layout(); if(!updating_ && w!=SIZE_MINIMIZED){CaptureUiState(); MarkDirty();} return 0;
    case WM_SETCURSOR:{
        POINT p{};GetCursorPos(&p);ScreenToClient(hwnd_,&p);RECT rr{};GetClientRect(hwnd_,&rr);int left=520;
        if(p.x>=left && std::abs(p.y-splitterY_)<=4){SetCursor(LoadCursor(nullptr,IDC_SIZENS));return TRUE;}
        break;
    }
    case WM_LBUTTONDOWN:{
        POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)}; if(p.x>=520 && std::abs(p.y-splitterY_)<=5){splitterDrag_=true;SetCapture(hwnd_);return 0;} break;
    }
    case WM_MOUSEMOVE:
        if(splitterDrag_ && (w&MK_LBUTTON)){RECT rr{};GetClientRect(hwnd_,&rr);int top=48,bottom=(int)rr.bottom-8,usable=std::max(100,bottom-top-8);int y=std::clamp(GET_Y_LPARAM(l),top+80,bottom-80);prj_.rightListPercent=std::clamp((y-top)*100/usable,20,85);Layout();MarkDirty();return 0;}
        break;
    case WM_LBUTTONUP:
        if(splitterDrag_){splitterDrag_=false;if(GetCapture()==hwnd_)ReleaseCapture();CaptureUiState();MarkDirty();return 0;} break;
    case WM_CAPTURECHANGED: splitterDrag_=false; break;
    case WM_PAINT:{
        PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd_,&ps);RECT rr{};GetClientRect(hwnd_,&rr);
        HBRUSH bg=CreateSolidBrush(GetSysColor(COLOR_WINDOW));FillRect(dc,&ps.rcPaint,bg);DeleteObject(bg);
        RECT sp{520,splitterY_-3,rr.right-8,splitterY_+4};HBRUSH br=CreateSolidBrush(GetSysColor(COLOR_3DFACE));FillRect(dc,&sp,br);DeleteObject(br);DrawEdge(dc,&sp,EDGE_ETCHED,BF_TOP|BF_BOTTOM);EndPaint(hwnd_,&ps);return 0;
    }
    case WM_DROPFILES: AddDropped((HDROP)w); return 0;
    case WM_TIMER:
        if(w==2){
            // Waveform animation timer (~60 fps).  The WAV-list waveform itself
            // is fixed; only the playback-position bar advances.
            if(!listDisplayWave_.empty()){
                if(mousePlayer_.IsPlaying()) InvalidateListBody(list_);
                else{ listDisplayWave_.clear(); InvalidateListBody(list_);UpdateWindow(list_); }
            }
            bool keyActive=!mouseKeyDisplayWave_.empty() && mousePlayer_.IsPlaying();
            for(int i=0;i<12 && !keyActive;++i) keyActive=!keyDisplayWave_[i].empty() && keyPlayers_[i].IsPlaying();
            for(int i=0;i<96 && !keyActive;++i) keyActive=!midiDisplayWave_[i].empty() && midiPlayers_[i].IsPlaying();
            if(keyActive) InvalidateRect(keys_,nullptr,FALSE);
            return 0;
        }
        // ListView can alter mouse capture internally. If button-up was not
        // delivered to the subclass, finish the preview/drop here as a fallback.
        if(listHolding_ && !(GetAsyncKeyState(VK_LBUTTON)&0x8000)) FinishListGesture();
        if(!mousePlayer_.IsPlaying()){
            if(!listDisplayWave_.empty()){listDisplayWave_.clear();InvalidateListBody(list_);UpdateWindow(list_);}
            if(!mouseKeyDisplayWave_.empty()){mouseKeyDisplayWave_.clear();InvalidateRect(keys_,nullptr,TRUE);}
        }
        for(int i=0;i<12;++i)if(!keyDisplayWave_[i].empty()&&!keyPlayers_[i].IsPlaying()){keyDisplayWave_[i].clear();InvalidateRect(keys_,nullptr,TRUE);}
        for(int i=0;i<96;++i)if(!midiDisplayWave_[i].empty()&&!midiPlayers_[i].IsPlaying()){midiDisplayWave_[i].clear();InvalidateRect(keys_,nullptr,TRUE);}
        SaveAuto(); return 0;
    case WM_APP+1: HandleMidi((DWORD)w); return 0;
    case WM_COMMAND:{
        int id=LOWORD(w), code=HIWORD(w);
        if(id==ID_CONVERT) Convert();
        else if(id==ID_LOAD&&code==BN_CLICKED){
            OPENFILENAMEW o{sizeof(o)}; wchar_t b[MAX_PATH]{}; o.hwndOwner=hwnd_; o.lpstrFile=b; o.nMaxFile=MAX_PATH;
            auto filter=MakeOpenFilter(); o.lpstrFilter=filter.data();
            o.Flags=OFN_FILEMUSTEXIST;
            if(GetOpenFileNameW(&o)){
                fs::path fp(b);
                if(!_wcsicmp(fp.extension().c_str(),L".pdx")) ImportPdx(b);
                else LoadProjectFile(b,false);
            }
        }
        else if(id==ID_FMT&&code==CBN_SELCHANGE&&!updating_){ PushUndo(); prj_.format=(PcmFormat)SendMessage(fmt_,CB_GETCURSEL,0,0); for(auto& b:prj_.banks)for(auto& s:b.slot)s.importedRaw=false; MarkDirty(); }
        else if(id==ID_NAME&&code==EN_CHANGE&&!updating_){ wchar_t b[260]{}; GetWindowTextW(name_,b,260); prj_.pdxName=b; MarkDirty(); }
        else if(id==ID_BANK&&code==CBN_SELCHANGE&&!updating_) SwitchBank((int)SendMessage(bankCombo_,CB_GETCURSEL,0,0));
        else if(id==ID_MIDITOGGLE&&code==BN_CLICKED)OpenMidi();
        else if(id==ID_MIDICOMBO&&code==CBN_SELCHANGE&&IsDlgButtonChecked(hwnd_,ID_MIDITOGGLE))OpenMidi();
        else if(id==ID_LANGUAGE&&code==CBN_DROPDOWN&&!updating_){LoadLanguagePacks(AppDirectory());ReloadLanguageCombo();}
        else if(id==ID_LANGUAGE&&code==CBN_SELCHANGE&&!updating_){ prj_.language=(int)SendMessage(languageCombo_,CB_GETCURSEL,0,0); SelectLanguageIndex(prj_.language); prj_.languageCode=gLanguagePacks[gLanguageIndex].code; ApplyLanguage(); RebuildFileList(); MarkDirty(); }
        else if((id==ID_VOLEDIT||id==ID_TREDIT)&&code==EN_SETFOCUS)BeginControlEdit();
        else if((id==ID_VOLEDIT||id==ID_TREDIT)&&code==EN_CHANGE)ReadControls();
        else if((id==ID_VOLEDIT||id==ID_TREDIT)&&code==EN_KILLFOCUS)EndControlEdit();
        else if(id==ID_STRETCH&&code==BN_CLICKED){PushUndo(); ReadControls(); Log(Tr(L"s025")+std::to_wstring(prj_.currentBank)+L" PDX "+std::to_wstring(selected_));}
        return 0;
    }
    case WM_HSCROLL:
        if((HWND)l==waveScroll_){
            SCROLLINFO si{sizeof(si),SIF_ALL}; GetScrollInfo(waveScroll_,SB_CTL,&si);
            int pos=si.nPos; int code=LOWORD(w);
            if(code==SB_THUMBTRACK||code==SB_THUMBPOSITION)pos=si.nTrackPos;
            else if(code==SB_LINELEFT)pos-=2; else if(code==SB_LINERIGHT)pos+=2;
            else if(code==SB_PAGELEFT)pos-=(int)std::max<UINT>(1,si.nPage); else if(code==SB_PAGERIGHT)pos+=(int)std::max<UINT>(1,si.nPage);
            ScrollWaveTo(pos); return 0;
        }
        if((HWND)l==previewTrack_){
            int v=(int)SendMessage(previewTrack_,TBM_GETPOS,0,0);
            prj_.previewVolume=std::clamp(v,0,500);
            wchar_t b[64]; swprintf_s(b,Tr(L"s026"),prj_.previewVolume); SetWindowTextW(previewLabel_,b);
            MarkDirty();
            return 0;
        }
        if((HWND)l==volTrack_||(HWND)l==trTrack_){
            int code=LOWORD(w); if(code==TB_THUMBTRACK||code==TB_THUMBPOSITION||code==TB_LINEUP||code==TB_LINEDOWN||code==TB_PAGEUP||code==TB_PAGEDOWN){ if(!pendingControlEdit_)BeginControlEdit(); }
            int v=(int)SendMessage((HWND)l,TBM_GETPOS,0,0); wchar_t b[32];
            if((HWND)l==volTrack_)swprintf_s(b,L"%d",v); else swprintf_s(b,L"%+d",v);
            SetWindowTextW((HWND)l==volTrack_?volEdit_:trEdit_,b); ReadControls();
            if(code==TB_ENDTRACK||code==TB_THUMBPOSITION)EndControlEdit();
        }
        return 0;
    case WM_NOTIFY:{
        auto* n=(NMHDR*)l;
        if(n->hwndFrom==list_&&n->code==LVN_COLUMNCLICK){SortList(((NMLISTVIEW*)l)->iSubItem);return 0;}
        if(n->hwndFrom==ListView_GetHeader(list_)&&n->code==HDN_ENDTRACKW){CaptureUiState();MarkDirty();return 0;}
        break;
    }
    case WM_KEYDOWN:
    case WM_KEYUP:
        if(!FocusIsText()){
            bool down=m==WM_KEYDOWN;
            HWND focus=GetFocus();
            if(focus==list_ || IsChild(list_,focus)){
                if(down && (GetKeyState(VK_CONTROL)&0x8000) && (w=='A')){
                    ListView_SetItemState(list_,-1,LVIS_SELECTED,LVIS_SELECTED);
                    return 0;
                }
                if(down && w==VK_DELETE){DeleteFocused();return 0;}
                if(w==VK_RETURN){
                    if(down){
                        if(!listEnterPreview_ && !(l&(1LL<<30))){
                            int row=ListView_GetNextItem(list_,-1,LVNI_FOCUSED);
                            if(row<0) row=ListView_GetSelectionMark(list_);
                            if(row>=0 && row<(int)files_.size()){
                                WaveData wd;
                                if(GetWave(files_[row],wd)){
                                    mousePlayer_.Stop(); listDisplayWave_=wd.mono; listDisplayRate_=wd.sampleRate;
                                    auto pv=ApplyPreviewGain(wd.mono);
                                    mousePlayer_.Play(pv,wd.sampleRate); InvalidateListBody(list_);
                                    listEnterPreview_=true;
                                }
                            }
                        }
                    }else if(listEnterPreview_){
                        mousePlayer_.Stop(); listDisplayWave_.clear(); InvalidateListBody(list_);UpdateWindow(list_);
                        listEnterPreview_=false;
                    }
                    return 0;
                }
                if(w==VK_UP||w==VK_DOWN||w==VK_HOME||w==VK_END||w==VK_PRIOR||w==VK_NEXT||w==VK_SPACE){
                    SendMessage(list_,m,w,l);
                    return 0;
                }
            }
            if(focus==keys_){
                if(w==VK_LEFT||w==VK_RIGHT||w==VK_UP||w==VK_DOWN){
                    if(down){
                        int delta=(w==VK_LEFT?-1:w==VK_RIGHT?1:w==VK_UP?-12:12);
                        int next=selected_+delta;
                        if(ValidPdx(next)) SelectKey(next,false);
                    }
                    return 0;
                }
                if(w==VK_RETURN){
                    if(down){
                        if(!keyEnterPreview_ && !(l&(1LL<<30))){
                            PreviewKey(selected_);
                            keyEnterPreview_=true;
                            InvalidateRect(keys_,nullptr,FALSE);
                        }
                    }else if(keyEnterPreview_){
                        StopPreview();
                        keyEnterPreview_=false;
                        InvalidateRect(keys_,nullptr,FALSE);
                    }
                    return 0;
                }
            }
            if(down && (GetKeyState(VK_CONTROL)&0x8000) && (w=='Z')){
                if(GetKeyState(VK_SHIFT)&0x8000)Redo(); else Undo(); return 0;
            }
            if(down && (GetKeyState(VK_CONTROL)&0x8000) && (w=='Y')){ Redo(); return 0; }
            if(down && w==VK_DELETE){DeleteFocused();return 0;}
            const wchar_t* map=L"AWSEDFTGYHUJ"; int sem=-1;
            for(int i=0;i<12;++i)if((wchar_t)w==map[i]){sem=i;break;}
            if(sem>=0){KeyboardNote(sem,down);return 0;}
            if(down && (w==VK_UP||w==VK_DOWN)){
                int delta=(w==VK_UP?-12:12),next=selected_+delta;
                if(ValidPdx(next))SelectKey(next,false);
                return 0;
            }
        }
        break;
    case WM_DESTROY:
        CloseMidi(); mousePlayer_.Stop(); for(auto& p:keyPlayers_)p.Stop(); CaptureUiState(); if(dirty_)SaveCurrentNow(); PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd_,m,w,l);
}

void App::CreateUI(){
    font_=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
    auto C=[&](DWORD ex,const wchar_t*c,const wchar_t*t,DWORD s,int id){HWND h=CreateWindowExW(ex,c,t,s|WS_CHILD|WS_VISIBLE,0,0,10,10,hwnd_,(HMENU)(INT_PTR)id,inst_,nullptr);SendMessage(h,WM_SETFONT,(WPARAM)font_,TRUE);return h;};
    pdxNameLabel_=C(0,L"STATIC",Tr(L"s027"),SS_LEFTNOWORDWRAP,0); name_=C(WS_EX_CLIENTEDGE,L"EDIT",L"pcm",ES_AUTOHSCROLL,ID_NAME);
    formatLabel_=C(0,L"STATIC",Tr(L"s028"),SS_LEFT,0); fmt_=C(0,WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL,ID_FMT);
    for(auto*s:{Tr(L"s095"),Tr(L"s096"),Tr(L"s097")})SendMessage(fmt_,CB_ADDSTRING,0,(LPARAM)s); SendMessage(fmt_,CB_SETCURSEL,0,0);
    loadBtn_=C(0,L"BUTTON",Tr(L"s029"),BS_PUSHBUTTON,ID_LOAD); convertBtn_=C(0,L"BUTTON",Tr(L"s030"),BS_DEFPUSHBUTTON,ID_CONVERT);
    bankLabel_=C(0,L"STATIC",Tr(L"s093"),SS_LEFT,0); bankCombo_=C(0,WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL,ID_BANK);
    midiToggle_=C(0,L"BUTTON",Tr(L"s031"),BS_AUTOCHECKBOX,ID_MIDITOGGLE); midiCombo_=C(0,WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL,ID_MIDICOMBO);
    keys_=CreateWindowExW(WS_EX_CLIENTEDGE,kKeys,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,10,10,hwnd_,nullptr,inst_,nullptr);
    list_=C(WS_EX_CLIENTEDGE,WC_LISTVIEWW,L"",LVS_REPORT|LVS_SHOWSELALWAYS,ID_LIST);
    ListView_SetExtendedListViewStyle(list_,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_HEADERDRAGDROP);
    const wchar_t* cols[]={Tr(L"s032"),Tr(L"s033"),Tr(L"s034"),Tr(L"s082"),Tr(L"s083"),Tr(L"s035"),Tr(L"s036"),Tr(L"s037"),Tr(L"s038")};
    for(int i=0;i<9;++i){LVCOLUMNW c{LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM};c.pszText=(LPWSTR)cols[i];c.cx=prj_.listWidths[i];c.iSubItem=i;ListView_InsertColumn(list_,i,&c);} gOldList=(WNDPROC)SetWindowLongPtr(list_,GWLP_WNDPROC,(LONG_PTR)ListProc);
    log_=C(WS_EX_CLIENTEDGE,L"EDIT",L"",ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,ID_LOG);
    slotLabel_=C(0,L"STATIC",L"PDX 00",SS_LEFT,0); volumeLabel_=C(0,L"STATIC",Tr(L"s039"),SS_LEFT,0);
    volTrack_=C(0,TRACKBAR_CLASSW,L"",TBS_HORZ,ID_VOLTR);SendMessage(volTrack_,TBM_SETRANGE,TRUE,MAKELONG(0,200));volEdit_=C(WS_EX_CLIENTEDGE,L"EDIT",L"100",ES_NUMBER,ID_VOLEDIT);
    transposeLabel_=C(0,L"STATIC",Tr(L"s094"),SS_LEFT,0); trTrack_=C(0,TRACKBAR_CLASSW,L"",TBS_HORZ,ID_TRTR);SendMessage(trTrack_,TBM_SETRANGE,TRUE,MAKELONG(-24,24));trEdit_=C(WS_EX_CLIENTEDGE,L"EDIT",L"0",ES_AUTOHSCROLL,ID_TREDIT);
    stretch_=C(0,L"BUTTON",Tr(L"s040"),BS_AUTOCHECKBOX,ID_STRETCH);
    waveEdit_=CreateWindowExW(WS_EX_CLIENTEDGE,kWave,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,10,10,hwnd_,nullptr,inst_,nullptr);
    waveScroll_=C(0,L"SCROLLBAR",L"",SBS_HORZ,ID_WAVESCROLL);
    previewLabel_=C(0,L"STATIC",Tr(L"s041"),SS_LEFT,0);
    previewTrack_=C(0,TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_AUTOTICKS,ID_PREVIEWTR); SendMessage(previewTrack_,TBM_SETRANGE,TRUE,MAKELONG(0,500)); SendMessage(previewTrack_,TBM_SETTICFREQ,50,0); SendMessage(previewTrack_,TBM_SETPOS,TRUE,100);
    languageLabel_=C(0,L"STATIC",Tr(L"s042"),SS_LEFT,0); languageCombo_=C(0,WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL,ID_LANGUAGE); ReloadLanguageCombo();
    RefreshMidi(); RefreshBankCombo(); selected_=0; UpdateControls(); ApplyLanguage();
    // A project passed on the command line (for example by dropping a .pdxedit
    // file onto PDXEditor.exe) takes priority over the last-project marker.
    bool startupLoaded=false;
    if(!startupProject_.empty()){
        fs::path sp=startupProject_;
        if(fs::exists(sp)){
            if(!_wcsicmp(sp.extension().c_str(),L".pdxedit")){LoadProjectFile(fs::absolute(sp).wstring(),false);startupLoaded=true;}
            else if(!_wcsicmp(sp.extension().c_str(),L".pdx")){ImportPdx(fs::absolute(sp).wstring());startupLoaded=true;}
        }
    }
    // Load the last project before emitting any localized startup message.
    // The marker lives beside the executable so changing the working directory
    // for a project does not create a different marker per project folder.
    if(!startupLoaded){
        std::ifstream lf(LastProjectMarkerPath(),std::ios::binary);
        if(lf){std::string u;std::getline(lf,u);auto lp=Utf8ToW(u);if(!lp.empty()&&fs::exists(lp))LoadProjectFile(lp,false);}
    }
    ApplyLanguage();
    Log(Tr(L"s043"));
}



void App::ReloadLanguageCombo(){
    if(!languageCombo_)return;
    updating_=true;SendMessage(languageCombo_,CB_RESETCONTENT,0,0);
    for(const auto& lp:gLanguagePacks)SendMessage(languageCombo_,CB_ADDSTRING,0,(LPARAM)lp.name.c_str());
    int idx=-1;if(!prj_.languageCode.empty())idx=LanguageIndexByCode(prj_.languageCode);
    if(idx<0 && prj_.language>=0 && prj_.language<(int)gLanguagePacks.size())idx=prj_.language;
    if(idx<0)idx=0;prj_.language=idx;prj_.languageCode=gLanguagePacks[idx].code;SelectLanguageIndex(idx);
    SendMessage(languageCombo_,CB_SETCURSEL,idx,0);updating_=false;
}

std::vector<float> App::BuildScrollingWindow(const std::vector<float>& wave,size_t pos,uint32_t rate) const{
    if(wave.empty()||rate==0)return{};
    const size_t window=std::max<size_t>(256,size_t(rate/2)); // about 0.5 second
    std::vector<float> out(window,0.f);
    pos=std::min(pos,wave.size());
    size_t count=std::min(pos,window);
    if(count)std::copy(wave.begin()+(pos-count),wave.begin()+pos,out.begin()+(window-count));
    return out;
}

std::vector<float> App::BuildKeyDisplayMix() const{
    const size_t window=15625/2;
    std::vector<float> mix(window,0.f); bool any=false;
    auto addCurrent=[&](const std::vector<float>&v,const WaveOutPlayer&p){
        if(v.empty()||!p.IsPlaying())return; auto w=BuildScrollingWindow(v,p.PositionSamples(),15625);
        if(w.empty())return; any=true; for(size_t i=0;i<std::min(mix.size(),w.size());++i)mix[i]+=w[i];
    };
    addCurrent(mouseKeyDisplayWave_,mousePlayer_);
    for(int i=0;i<12;++i)addCurrent(keyDisplayWave_[i],keyPlayers_[i]);
    for(int i=0;i<96;++i)addCurrent(midiDisplayWave_[i],midiPlayers_[i]);
    if(!any)return{};for(auto&x:mix)x=std::clamp(x,-1.f,1.f);return mix;
}

void App::DrawWaveOverlay(HDC dc,const RECT& rc,const std::vector<float>& wave){
    if(wave.empty()||rc.right<=rc.left+2||rc.bottom<=rc.top+2)return;
    int w=rc.right-rc.left,h=rc.bottom-rc.top;BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void*bits=nullptr;HBITMAP bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(!bmp||!bits){if(bmp)DeleteObject(bmp);return;}
    auto*px=(uint32_t*)bits;std::fill(px,px+(size_t)w*h,0u);const uint8_t a=105;const uint8_t rr=0,gg=(uint8_t)(150*a/255),bb=(uint8_t)(255*a/255);uint32_t col=(uint32_t(a)<<24)|(uint32_t(rr)<<16)|(uint32_t(gg)<<8)|bb;
    auto put=[&](int x,int y){if(x>=0&&x<w&&y>=0&&y<h)px[(size_t)y*w+x]=col;};int cy=h/2;int amp=std::max(1,h/2-4);int py=cy;
    for(int x=0;x<w;++x){size_t i=(size_t)((uint64_t)x*wave.size()/std::max(1,w));if(i>=wave.size())i=wave.size()-1;int y=cy-(int)std::lround(std::clamp(wave[i],-1.f,1.f)*amp);int y0=std::min(py,y),y1=std::max(py,y);for(int yy=y0;yy<=y1;++yy)put(x,yy);py=y;}
    HDC mem=CreateCompatibleDC(dc);HGDIOBJ old=SelectObject(mem,bmp);BLENDFUNCTION bf{AC_SRC_OVER,0,255,AC_SRC_ALPHA};AlphaBlend(dc,rc.left,rc.top,w,h,mem,0,0,w,h,bf);SelectObject(mem,old);DeleteDC(mem);DeleteObject(bmp);
}

void App::DrawPlaybackBar(HDC dc,const RECT& rc,double fraction){
    if(rc.right<=rc.left||rc.bottom<=rc.top)return;
    fraction=std::clamp(fraction,0.0,1.0);
    int x=rc.left+(int)std::lround((rc.right-rc.left-1)*fraction);
    RECT bar{x-2,rc.top,x+3,rc.bottom};
    int w=std::max(1,(int)(bar.right-bar.left)),h=std::max(1,(int)(bar.bottom-bar.top));
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void*bits=nullptr;HBITMAP bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(!bmp||!bits){if(bmp)DeleteObject(bmp);return;}
    const uint8_t a=145;const uint8_t rr=(uint8_t)(255*a/255),gg=(uint8_t)(145*a/255),bb=0;uint32_t col=(uint32_t(a)<<24)|(uint32_t(rr)<<16)|(uint32_t(gg)<<8)|bb;
    auto*px=(uint32_t*)bits;std::fill(px,px+(size_t)w*h,col);HDC mem=CreateCompatibleDC(dc);HGDIOBJ old=SelectObject(mem,bmp);BLENDFUNCTION bf{AC_SRC_OVER,0,255,AC_SRC_ALPHA};AlphaBlend(dc,bar.left,bar.top,w,h,mem,0,0,w,h,bf);SelectObject(mem,old);DeleteDC(mem);DeleteObject(bmp);
}

void App::ApplyLanguage(){
    SelectLanguageIndex(prj_.language); if(prj_.language>=0&&prj_.language<(int)gLanguagePacks.size())prj_.languageCode=gLanguagePacks[prj_.language].code;
    SetWindowTextW(hwnd_,Tr(L"s044"));
    SetWindowTextW(pdxNameLabel_,Tr(L"s027"));
    SetWindowLongPtrW(pdxNameLabel_,GWL_STYLE,(GetWindowLongPtrW(pdxNameLabel_,GWL_STYLE)&~SS_TYPEMASK)|SS_LEFTNOWORDWRAP);
    SetWindowTextW(formatLabel_,Tr(L"s028"));
    SetWindowLongPtrW(formatLabel_,GWL_STYLE,(GetWindowLongPtrW(formatLabel_,GWL_STYLE)&~SS_TYPEMASK)|SS_LEFTNOWORDWRAP);
    SetWindowTextW(loadBtn_,Tr(L"s029"));
    SetWindowTextW(convertBtn_,Tr(L"s030"));
    SetWindowTextW(midiToggle_,Tr(L"s031"));
    SetWindowTextW(volumeLabel_,Tr(L"s039"));
    SetWindowTextW(bankLabel_,Tr(L"s093"));
    SetWindowTextW(transposeLabel_,Tr(L"s094"));
    int oldFmt=(int)SendMessage(fmt_,CB_GETCURSEL,0,0);SendMessage(fmt_,CB_RESETCONTENT,0,0);for(auto* q:{Tr(L"s095"),Tr(L"s096"),Tr(L"s097")})SendMessage(fmt_,CB_ADDSTRING,0,(LPARAM)q);SendMessage(fmt_,CB_SETCURSEL,oldFmt<0?(int)prj_.format:oldFmt,0);
    SetWindowTextW(stretch_,Tr(L"s040"));
    SetWindowTextW(languageLabel_,Tr(L"s042"));
    const wchar_t* cols[]={Tr(L"s032"),Tr(L"s033"),Tr(L"s034"),Tr(L"s082"),Tr(L"s083"),Tr(L"s035"),Tr(L"s036"),Tr(L"s037"),Tr(L"s038")};
    HDC ldc=GetDC(list_);HGDIOBJ lold=SelectObject(ldc,font_);
    for(int i=0;i<9;++i){LVCOLUMNW c{LVCF_TEXT};c.pszText=(LPWSTR)cols[i];ListView_SetColumn(list_,i,&c);SIZE z{};GetTextExtentPoint32W(ldc,cols[i],(int)wcslen(cols[i]),&z);int need=z.cx+28;if(ListView_GetColumnWidth(list_,i)<need)ListView_SetColumnWidth(list_,i,need);}
    SelectObject(ldc,lold);ReleaseDC(list_,ldc);
    wchar_t b[64];swprintf_s(b,Tr(L"s026"),prj_.previewVolume);SetWindowTextW(previewLabel_,b);
    updating_=true;SendMessage(languageCombo_,CB_SETCURSEL,prj_.language,0);updating_=false;
    // Recalculate control widths after changing language.  English labels are
    // generally wider than the Japanese ones, so fixed widths can clip text.
    if(hwnd_ && pdxNameLabel_){
        Layout();
        // Language changes can move several transparent STATIC controls.
        // Repaint the parent and all children so pixels from the old positions
        // cannot remain behind (notably around the Format label).
        RedrawWindow(hwnd_,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_ALLCHILDREN|RDW_UPDATENOW);
    }
}

void App::Layout(){
    if(!hwnd_||!keys_)return; RECT r{};GetClientRect(hwnd_,&r);int W=(int)r.right,H=(int)r.bottom;
    int y=10;
    auto textWidth=[&](HWND ctl,int extra)->int{
        wchar_t text[256]{}; GetWindowTextW(ctl,text,256); HDC dc=GetDC(ctl); HGDIOBJ old=SelectObject(dc,font_);
        SIZE sz{}; GetTextExtentPoint32W(dc,text,(int)wcslen(text),&sz); SelectObject(dc,old); ReleaseDC(ctl,dc); return (int)sz.cx+extra;
    };

    int x=8;
    int pdxLabelW=std::max(52,textWidth(pdxNameLabel_,8)); MoveWindow(pdxNameLabel_,x,15,pdxLabelW,20,TRUE); x+=pdxLabelW+4;
    MoveWindow(name_,x,y,125,26,TRUE); x+=131;
    int fmtLabelW=std::max(42,textWidth(formatLabel_,8)); MoveWindow(formatLabel_,x,15,fmtLabelW,20,TRUE); x+=fmtLabelW+4;
    int fmtW=130; {HDC dc=GetDC(fmt_);HGDIOBJ old=SelectObject(dc,font_);int n=(int)SendMessage(fmt_,CB_GETCOUNT,0,0);for(int i=0;i<n;++i){wchar_t b[256]{};SendMessage(fmt_,CB_GETLBTEXT,i,(LPARAM)b);SIZE z{};GetTextExtentPoint32W(dc,b,(int)wcslen(b),&z);fmtW=std::max(fmtW,(int)z.cx+34);}SelectObject(dc,old);ReleaseDC(fmt_,dc);} MoveWindow(fmt_,x,y,fmtW,120,TRUE); x+=fmtW+6;
    int loadW=std::max(100,textWidth(loadBtn_,22)); MoveWindow(loadBtn_,x,y,loadW,28,TRUE); x+=loadW+4;
    int convertW=std::max(118,textWidth(convertBtn_,22)); MoveWindow(convertBtn_,x,y,convertW,28,TRUE); x+=convertW+7;
    int bankLabelW=std::max(34,textWidth(bankLabel_,6)); MoveWindow(bankLabel_,x,15,bankLabelW,20,TRUE); x+=bankLabelW+3; MoveWindow(bankCombo_,x,y,78,140,TRUE); x+=84;
    int midiToggleW=std::max(110,textWidth(midiToggle_,30)); MoveWindow(midiToggle_,x,12,midiToggleW,24,TRUE); x+=midiToggleW+4;
    int reserveLang=220; int midiW=std::max(115,std::min(180,W-x-reserveLang)); MoveWindow(midiCombo_,x,y,midiW,140,TRUE); x+=midiW+7;
    int langW=std::max(58,textWidth(languageLabel_,8)); MoveWindow(languageLabel_,x,15,langW,20,TRUE); x+=langW+4;
    MoveWindow(languageCombo_,x,y,std::max(90,W-x-8),120,TRUE);

    const int top=48,gap=8,left=520; const int rightX=left,rightW=std::max(200,W-rightX-8); const int bottom=H-8;
    // Right pane is reserved exclusively for the WAV list and log.
    int usableRight=std::max(180,bottom-top-gap); int listH=usableRight*std::clamp(prj_.rightListPercent,20,85)/100;
    listH=std::clamp(listH,80,std::max(80,usableRight-80)); int oldSplitterY=splitterY_; splitterY_=top+listH+gap/2;
    if(oldSplitterY>0){RECT oldSp{rightX,oldSplitterY-5,W-8,oldSplitterY+6};InvalidateRect(hwnd_,&oldSp,TRUE);}RECT newSp{rightX,splitterY_-5,W-8,splitterY_+6};InvalidateRect(hwnd_,&newSp,TRUE);
    MoveWindow(list_,rightX,top,rightW,listH,TRUE);
    MoveWindow(log_,rightX,top+listH+gap,rightW,std::max(40,bottom-(top+listH+gap)),TRUE);

    // Left pane: keyboard on top, all per-key/sample settings below it.
    const int settingsH=292; int keyH=std::max(180,H-top-settingsH-gap); MoveWindow(keys_,8,top,left-16,keyH,TRUE);
    int sy=top+keyH+gap; int leftW=left-16;
    int slotW=std::max(250,textWidth(slotLabel_,10)); MoveWindow(slotLabel_,8,sy,std::min(slotW,leftW),22,TRUE);
    int volumeW=std::max(55,textWidth(volumeLabel_,8)); MoveWindow(volumeLabel_,8,sy+28,volumeW,20,TRUE); MoveWindow(volTrack_,8+volumeW+5,sy+22,170,32,TRUE); MoveWindow(volEdit_,8+volumeW+180,sy+23,52,24,TRUE);
    int stretchW=std::max(120,textWidth(stretch_,30)); MoveWindow(stretch_,8+volumeW+240,sy+25,std::min(stretchW,std::max(80,leftW-(volumeW+240))),24,TRUE);
    int transW=std::max(75,textWidth(transposeLabel_,8)); MoveWindow(transposeLabel_,8,sy+62,transW,20,TRUE); MoveWindow(trTrack_,8+transW+5,sy+55,170,32,TRUE); MoveWindow(trEdit_,8+transW+180,sy+57,52,24,TRUE);
    int prevW=std::max(105,textWidth(previewLabel_,8)); int px=8+transW+240; MoveWindow(previewLabel_,px,sy+61,std::min(prevW,std::max(80,leftW-(px-8))),20,TRUE); MoveWindow(previewTrack_,px,sy+78,std::max(80,leftW-(px-8)),28,TRUE);
    // Horizontal view scrollbar sits immediately above the waveform editor.
    MoveWindow(waveScroll_,8,sy+101,leftW,18,TRUE);
    MoveWindow(waveEdit_,8,sy+120,leftW,std::max(120,bottom-(sy+120)),TRUE);
    UpdateWaveScroll();
}

static RECT WhiteRectFor(int x0,int y,int whiteW,int h,int whiteIndex){ return RECT{x0+whiteIndex*whiteW,y,x0+(whiteIndex+1)*whiteW,y+h}; }
static int WhiteIndexForSemi(int s){ static const int wi[12]={0,0,1,1,2,3,3,4,4,5,5,6}; return wi[s]; }

void App::PaintKeys(HDC dc){
    RECT rc{};GetClientRect(keys_,&rc);
    HBRUSH bg=CreateSolidBrush(RGB(34,34,34));FillRect(dc,&rc,bg);DeleteObject(bg);
    int labelW=42; int rowH=std::max(34,(int)rc.bottom/9); int whiteW=std::max(24,((int)rc.right-labelW-10)/7);
    SetBkMode(dc,TRANSPARENT); SelectObject(dc,font_);
    for(int oct=0;oct<9;++oct){
        int y=oct*rowH;
        bool activeOct=(oct==prj_.internalOctave);
        if(activeOct){
            RECT plate{1,y+1,rc.right-1,(LONG)std::min((int)rc.bottom,y+rowH-1)};
            HBRUSH pb=CreateSolidBrush(RGB(48,60,48));FillRect(dc,&plate,pb);DeleteObject(pb);
            HPEN pen=CreatePen(PS_SOLID,2,RGB(125,170,105));HGDIOBJ old=SelectObject(dc,pen);int lineY=std::min((int)rc.bottom-1,y+rowH-2);MoveToEx(dc,1,y+1,nullptr);LineTo(dc,(int)rc.right-1,y+1);MoveToEx(dc,1,lineY,nullptr);LineTo(dc,(int)rc.right-1,lineY);SelectObject(dc,old);DeleteObject(pen);
        }
        wchar_t ol[8];swprintf_s(ol,L"o%d",oct);SetTextColor(dc,activeOct?RGB(210,255,190):RGB(235,235,235));TextOutW(dc,6,y+rowH/2-8,ol,(int)wcslen(ol));
        int whiteSemis[7]={0,2,4,5,7,9,11};
        for(int wi=0;wi<7;++wi){
            int semi=whiteSemis[wi],idx=PitchToPdx(oct,semi);RECT k=WhiteRectFor(labelW,y+3,whiteW,rowH-7,wi);
            bool valid=ValidPdx(idx);bool assigned=valid&&!prj_.banks[prj_.currentBank].slot[idx].wavPath.empty();
            bool hot=valid&&(idx==selected_||midiDown_.count(idx)||(keyDownSemis_.count(semi)&&oct==prj_.internalOctave)||(keyHolding_&&idx==selected_));
            COLORREF c=!valid?RGB(48,48,48):hot?RGB(155,235,92):assigned?RGB(238,238,238):RGB(92,92,92);
            HBRUSH b=CreateSolidBrush(c);FillRect(dc,&k,b);DeleteObject(b);FrameRect(dc,&k,(HBRUSH)GetStockObject(BLACK_BRUSH));
            if(valid){wchar_t n[8];swprintf_s(n,L"%02d",idx);SetTextColor(dc,hot?RGB(15,15,15):(assigned?RGB(20,20,20):RGB(225,225,225)));DrawTextW(dc,n,-1,&k,DT_CENTER|DT_BOTTOM|DT_SINGLELINE);}
        }
        int blackSemis[5]={1,3,6,8,10};
        for(int bi=0;bi<5;++bi){
            int semi=blackSemis[bi],idx=PitchToPdx(oct,semi),wi=WhiteIndexForSemi(semi);int cx=labelW+(wi+1)*whiteW;RECT k{cx-whiteW/4,y+3,cx+whiteW/4,y+rowH*3/5};
            bool valid=ValidPdx(idx);bool assigned=valid&&!prj_.banks[prj_.currentBank].slot[idx].wavPath.empty();
            bool hot=valid&&(idx==selected_||midiDown_.count(idx)||(keyDownSemis_.count(semi)&&oct==prj_.internalOctave)||(keyHolding_&&idx==selected_));
            // Unassigned black keys are deliberately much lighter than assigned black keys.
            COLORREF c=!valid?RGB(38,38,38):hot?RGB(155,235,92):assigned?RGB(18,18,18):RGB(104,104,104);
            HBRUSH b=CreateSolidBrush(c);FillRect(dc,&k,b);DeleteObject(b);FrameRect(dc,&k,(HBRUSH)GetStockObject(BLACK_BRUSH));
            if(valid){wchar_t n[8];swprintf_s(n,L"%02d",idx);SetTextColor(dc,hot?RGB(15,15,15):RGB(255,255,255));DrawTextW(dc,n,-1,&k,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
        }
    }
    auto mix=BuildKeyDisplayMix();if(!mix.empty())DrawWaveOverlay(dc,rc,mix);
}
int App::HitKey(POINT p) const{
    RECT rc{};GetClientRect(keys_,&rc);int labelW=38,rowH=std::max(34,(int)rc.bottom/9),whiteW=std::max(24,((int)rc.right-labelW-8)/7);int oct=p.y/rowH;if(oct<0||oct>8)return-1;
    int blackSemis[5]={1,3,6,8,10};for(int s:blackSemis){int wi=WhiteIndexForSemi(s),cx=labelW+(wi+1)*whiteW;RECT k{cx-whiteW/4,oct*rowH+1,cx+whiteW/4,oct*rowH+rowH*3/5};if(PtInRect(&k,p)){int idx=PitchToPdx(oct,s);return ValidPdx(idx)?idx:-1;}}
    if(p.x<labelW)return-1;int wi=(p.x-labelW)/whiteW;if(wi<0||wi>=7)return-1;int semis[7]={0,2,4,5,7,9,11};int idx=PitchToPdx(oct,semis[wi]);return ValidPdx(idx)?idx:-1;
}

LRESULT App::OnKeys(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);RECT rc{};GetClientRect(h,&rc);HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,std::max(1,(int)rc.right),std::max(1,(int)rc.bottom));HGDIOBJ old=SelectObject(mem,bmp);PaintKeys(mem);BitBlt(dc,0,0,rc.right,rc.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps);return 0;}
    case WM_MOUSEWHEEL:{
        if(keyHolding_ && ValidPdx(selected_)){
            int steps=GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA;
            if(steps){ PushUndo(); auto& sl=CurSlot(); sl.transpose=std::clamp(sl.transpose+steps,-24,24); sl.importedRaw=false; UpdateControls(); MarkDirty(); PreviewKey(selected_); }
        }
        return 0;
    }
    case WM_LBUTTONDOWN:{SetFocus(h);POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};int idx=HitKey(p);if(idx>=0){SelectKey(idx,true);keyHolding_=true;SetCapture(h);InvalidateRect(h,nullptr,FALSE);}return 0;}
    case WM_LBUTTONUP: if(GetCapture()==h)ReleaseCapture(); keyHolding_=false; StopPreview(); InvalidateRect(h,nullptr,FALSE); return 0;
    case WM_RBUTTONDOWN:{
        SetFocus(h); POINT cp{GET_X_LPARAM(l),GET_Y_LPARAM(l)}; int idx=HitKey(cp); if(idx<0)return 0;
        SelectKey(idx,false);
        HMENU menu=CreatePopupMenu(); HMENU wav=CreatePopupMenu();
        AppendMenuW(menu,MF_STRING,1,Tr(L"s045"));
        if(CurSlot().wavPath.empty()) EnableMenuItem(menu,1,MF_BYCOMMAND|MF_GRAYED);
        if(files_.empty()) AppendMenuW(wav,MF_STRING|MF_GRAYED,1000,Tr(L"s046"));
        else for(size_t i=0;i<files_.size();++i) AppendMenuW(wav,MF_STRING,1000+(UINT)i,BaseName(files_[i]).c_str());
        AppendMenuW(menu,MF_POPUP,(UINT_PTR)wav,Tr(L"s047"));
        POINT sp=cp; ClientToScreen(h,&sp);
        UINT cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON|TPM_NONOTIFY,sp.x,sp.y,0,hwnd_,nullptr);
        if(cmd==1) DeleteFocused();
        else if(cmd>=1000 && cmd<1000+files_.size()) AssignSelected(files_[cmd-1000]);
        DestroyMenu(menu); return 0;
    }
    }
    return DefWindowProc(h,m,w,l);
}


std::vector<float> App::TrimForSlot(const WaveData& w,const SlotSetting& s) const{
    if(w.mono.empty())return{};
    size_t a=(size_t)std::min<uint64_t>(s.trimStart,w.mono.size());
    size_t b=s.trimEnd? (size_t)std::min<uint64_t>(s.trimEnd,w.mono.size()) : w.mono.size();
    if(b<a)std::swap(a,b); if(b<=a)return{};
    return std::vector<float>(w.mono.begin()+a,w.mono.begin()+b);
}
void App::ResetWaveView(){
    waveViewStart_=0; waveViewEnd_=1;
    if(!waveEdit_||prj_.banks.empty()||CurSlot().wavPath.empty()){UpdateWaveScroll();if(waveEdit_)InvalidateRect(waveEdit_,nullptr,FALSE);return;}
    WaveData w;if(GetWave(CurSlot().wavPath,w)&&!w.mono.empty())waveViewEnd_=(double)w.mono.size();
    UpdateWaveScroll(); InvalidateRect(waveEdit_,nullptr,FALSE);
}
void App::UpdateWaveScroll(){
    if(!waveScroll_)return; uint64_t n=0; if(!prj_.banks.empty()&&!CurSlot().wavPath.empty()){WaveData w;if(GetWave(CurSlot().wavPath,w))n=w.mono.size();}
    SCROLLINFO si{sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS}; si.nMin=0; si.nMax=10000;
    if(n==0){si.nPage=10001;si.nPos=0;EnableWindow(waveScroll_,FALSE);}else{
        double span=std::clamp(waveViewEnd_-waveViewStart_,1.0,(double)n); double frac=span/std::max(1.0,(double)n);
        si.nPage=(UINT)std::clamp((int)std::lround(frac*10001.0),1,10001);
        double movable=std::max(0.0,(double)n-span); si.nPos=movable>0.0?(int)std::lround(std::clamp(waveViewStart_/movable,0.0,1.0)*(10000-(int)si.nPage+1)):0; EnableWindow(waveScroll_,movable>0.5);
    }
    SetScrollInfo(waveScroll_,SB_CTL,&si,TRUE);
}
void App::ScrollWaveTo(int pos){
    if(CurSlot().wavPath.empty())return; WaveData w;if(!GetWave(CurSlot().wavPath,w)||w.mono.empty())return; double n=(double)w.mono.size();
    double span=std::clamp(waveViewEnd_-waveViewStart_,1.0,n); SCROLLINFO si{sizeof(si),SIF_ALL};GetScrollInfo(waveScroll_,SB_CTL,&si);
    int maxPos=std::max(0,si.nMax-(int)si.nPage+1); pos=std::clamp(pos,0,maxPos); double movable=std::max(0.0,n-span);
    waveViewStart_=maxPos>0?movable*double(pos)/double(maxPos):0.0; waveViewEnd_=waveViewStart_+span; UpdateWaveScroll(); InvalidateRect(waveEdit_,nullptr,FALSE);
}
uint64_t App::WaveSampleAtX(int x) const{
    if(!waveEdit_)return 0;RECT rc{};GetClientRect(waveEdit_,&rc);int left=8,right=std::max(left+1,(int)rc.right-8);x=std::clamp(x,left,right);
    double t=double(x-left)/double(right-left);return (uint64_t)std::llround(waveViewStart_+(waveViewEnd_-waveViewStart_)*t);
}
int App::WaveXForSample(uint64_t smp) const{
    RECT rc{};GetClientRect(waveEdit_,&rc);int left=8,right=std::max(left+1,(int)rc.right-8);double span=std::max(1.0,waveViewEnd_-waveViewStart_);
    double t=(double(smp)-waveViewStart_)/span;return left+(int)std::lround(std::clamp(t,0.0,1.0)*(right-left));
}
void App::PaintWaveEditor(HDC dc){
    RECT rc{};GetClientRect(waveEdit_,&rc);HBRUSH bg=CreateSolidBrush(GetSysColor(COLOR_WINDOW));FillRect(dc,&rc,bg);DeleteObject(bg);SetBkMode(dc,TRANSPARENT);SelectObject(dc,font_);
    if(prj_.banks.empty()||CurSlot().wavPath.empty()){SetTextColor(dc,GetSysColor(COLOR_GRAYTEXT));DrawTextW(dc,Tr(L"s084"),-1,&rc,DT_CENTER|DT_VCENTER|DT_SINGLELINE);return;}
    WaveData wd;if(!GetWave(CurSlot().wavPath,wd)||wd.mono.empty())return;auto& sl=CurSlot();uint64_t n=wd.mono.size();uint64_t a=std::min<uint64_t>(sl.trimStart,n),b=sl.trimEnd?std::min<uint64_t>(sl.trimEnd,n):n;if(b<a)std::swap(a,b);
    wchar_t info[256];swprintf_s(info,Tr(L"s085"),a,b,b-a);RECT ir{8,4,rc.right-8,24};SetTextColor(dc,GetSysColor(COLOR_WINDOWTEXT));DrawTextW(dc,info,-1,&ir,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS);
    RECT wr{8,28,rc.right-8,rc.bottom-8};if(wr.right<=wr.left||wr.bottom<=wr.top)return;HBRUSH wb=CreateSolidBrush(RGB(245,245,245));FillRect(dc,&wr,wb);DeleteObject(wb);FrameRect(dc,&wr,(HBRUSH)GetStockObject(GRAY_BRUSH));
    int cy=((int)wr.top+(int)wr.bottom)/2,amp=std::max(1,((int)wr.bottom-(int)wr.top)/2-3);HPEN pen=CreatePen(PS_SOLID,1,RGB(45,90,160));HGDIOBJ old=SelectObject(dc,pen);int prevY=cy;
    for(int x=wr.left;x<wr.right;++x){uint64_t sm=WaveSampleAtX(x);uint64_t sm2=WaveSampleAtX(x+1);sm=std::min<uint64_t>(sm,n-1);sm2=std::max<uint64_t>(sm+1,std::min<uint64_t>(sm2,n));float lo=1.f,hi=-1.f;for(uint64_t i=sm;i<sm2;++i){lo=std::min(lo,wd.mono[i]);hi=std::max(hi,wd.mono[i]);}int y1=cy-(int)std::lround(hi*amp),y2=cy-(int)std::lround(lo*amp);MoveToEx(dc,x,y1,nullptr);LineTo(dc,x,y2+1);prevY=y2;}SelectObject(dc,old);DeleteObject(pen);
    int xa=WaveXForSample(a),xb=WaveXForSample(b);RECT sel{std::min(xa,xb),wr.top,std::max(xa,xb),wr.bottom};
    if(sel.right>sel.left){
        int sw=sel.right-sel.left,shh=sel.bottom-sel.top;BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=sw;bi.bmiHeader.biHeight=-shh;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        void*bits=nullptr;HBITMAP bm=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(bm&&bits){const uint8_t al=48,ha=82;uint32_t col=(uint32_t(al)<<24)|(uint32_t(60*al/255)<<16)|(uint32_t(190*al/255)<<8)|uint32_t(90*al/255);uint32_t hcol=(uint32_t(ha)<<24)|(uint32_t(90*ha/255)<<16)|(uint32_t(170*ha/255)<<8)|uint32_t(105*ha/255);auto*px=(uint32_t*)bits;std::fill(px,px+(size_t)sw*shh,col);for(int yy=0;yy<shh;++yy)for(int xx=0;xx<sw;++xx)if(((xx+yy)%12)<=1)px[(size_t)yy*sw+xx]=hcol;HDC md=CreateCompatibleDC(dc);HGDIOBJ oo=SelectObject(md,bm);BLENDFUNCTION bf{AC_SRC_OVER,0,255,AC_SRC_ALPHA};AlphaBlend(dc,sel.left,sel.top,sw,shh,md,0,0,sw,shh,bf);SelectObject(md,oo);DeleteDC(md);}if(bm)DeleteObject(bm);
    }
    HPEN hp=CreatePen(PS_SOLID,2,RGB(20,150,50));old=SelectObject(dc,hp);MoveToEx(dc,xa,wr.top,nullptr);LineTo(dc,xa,wr.bottom);MoveToEx(dc,xb,wr.top,nullptr);LineTo(dc,xb,wr.bottom);SelectObject(dc,old);DeleteObject(hp);
}
void App::CommitWaveEdit(){
    if(!pendingWaveEdit_)return;undo_.push_back(*pendingWaveEdit_);if(undo_.size()>100)undo_.erase(undo_.begin());redo_.clear();pendingWaveEdit_.reset();CurSlot().importedRaw=false;MarkDirty();UpdateControls();
}
void App::FindZeroCross(bool startEdge,bool outward,bool recordUndo){
    if(CurSlot().wavPath.empty())return;WaveData wd;if(!GetWave(CurSlot().wavPath,wd)||wd.mono.size()<2)return;auto& sl=CurSlot();uint64_t n=wd.mono.size(),a=std::min<uint64_t>(sl.trimStart,n),b=sl.trimEnd?std::min<uint64_t>(sl.trimEnd,n):n;if(b<a)std::swap(a,b);uint64_t p=startEdge?a:b;
    auto cross=[&](uint64_t i){return i>0&&i<n&&((wd.mono[i-1]<=0&&wd.mono[i]>=0)||(wd.mono[i-1]>=0&&wd.mono[i]<=0));};
    int dir=startEdge?(outward?-1:1):(outward?1:-1);uint64_t q=p;for(uint64_t k=0;k<n;++k){if(dir<0){if(q<=1)break;--q;}else{if(q+1>=n)break;++q;}if(startEdge&&!outward&&q>b)break;if(!startEdge&&!outward&&q<a)break;if(cross(q)){if(recordUndo)PushUndo();if(startEdge)sl.trimStart=q;else sl.trimEnd=q;sl.importedRaw=false;MarkDirty();InvalidateRect(waveEdit_,nullptr,FALSE);return;}}
}
LRESULT App::OnWave(HWND h,UINT m,WPARAM w,LPARAM l){
    auto getBounds=[&](uint64_t& a,uint64_t& b,uint64_t& n)->bool{if(CurSlot().wavPath.empty())return false;WaveData wd;if(!GetWave(CurSlot().wavPath,wd)||wd.mono.empty())return false;n=wd.mono.size();a=std::min<uint64_t>(CurSlot().trimStart,n);b=CurSlot().trimEnd?std::min<uint64_t>(CurSlot().trimEnd,n):n;if(b<a)std::swap(a,b);return true;};
    switch(m){
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);RECT rc{};GetClientRect(h,&rc);HDC mem=CreateCompatibleDC(dc);HBITMAP bm=CreateCompatibleBitmap(dc,std::max(1,(int)rc.right),std::max(1,(int)rc.bottom));auto old=SelectObject(mem,bm);PaintWaveEditor(mem);BitBlt(dc,0,0,rc.right,rc.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bm);DeleteDC(mem);EndPaint(h,&ps);return 0;}
    case WM_SETCURSOR:{POINT p{};GetCursorPos(&p);ScreenToClient(h,&p);uint64_t a,b,n;if(getBounds(a,b,n)){int xa=WaveXForSample(a),xb=WaveXForSample(b);if(std::abs(p.x-xa)<=6||std::abs(p.x-xb)<=6){SetCursor(LoadCursor(nullptr,IDC_SIZEWE));return TRUE;}}break;}
    case WM_LBUTTONDOWN:{SetFocus(h);uint64_t a,b,n;if(!getBounds(a,b,n))return 0;int x=GET_X_LPARAM(l),xa=WaveXForSample(a),xb=WaveXForSample(b);pendingWaveEdit_=Snapshot();uint64_t sm=std::min<uint64_t>(WaveSampleAtX(x),n);if(std::abs(x-xa)<=6)waveDrag_=WaveDragMode::Start;else if(std::abs(x-xb)<=6)waveDrag_=WaveDragMode::End;else{waveDrag_=WaveDragMode::Range;waveDragAnchor_=sm;CurSlot().trimStart=sm;CurSlot().trimEnd=sm;}SetCapture(h);InvalidateRect(h,nullptr,FALSE);return 0;}
    case WM_MOUSEMOVE:if(waveDrag_!=WaveDragMode::None&&(w&MK_LBUTTON)){uint64_t a,b,n;if(!getBounds(a,b,n))return 0;uint64_t sm=std::min<uint64_t>(WaveSampleAtX(GET_X_LPARAM(l)),n);auto& sl=CurSlot();if(waveDrag_==WaveDragMode::Start)sl.trimStart=std::min<uint64_t>(sm,b);else if(waveDrag_==WaveDragMode::End)sl.trimEnd=std::max<uint64_t>(sm,a);else{sl.trimStart=std::min(waveDragAnchor_,sm);sl.trimEnd=std::max(waveDragAnchor_,sm);}InvalidateRect(h,nullptr,FALSE);return 0;}break;
    case WM_LBUTTONUP:if(waveDrag_!=WaveDragMode::None){if(GetCapture()==h)ReleaseCapture();waveDrag_=WaveDragMode::None;CommitWaveEdit();InvalidateRect(h,nullptr,FALSE);return 0;}break;
    case WM_MOUSEWHEEL:{uint64_t a,b,n;if(!getBounds(a,b,n))return 0;POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);double center=(double)WaveSampleAtX(p.x),span=std::max(32.0,waveViewEnd_-waveViewStart_);double factor=GET_WHEEL_DELTA_WPARAM(w)>0?0.8:1.25,newSpan=std::clamp(span*factor,32.0,(double)n);RECT rc{};GetClientRect(h,&rc);double t=std::clamp(double(p.x-8)/std::max(1.0,double(rc.right-16)),0.0,1.0);double ns=center-newSpan*t;ns=std::clamp(ns,0.0,std::max(0.0,double(n)-newSpan));waveViewStart_=ns;waveViewEnd_=ns+newSpan;UpdateWaveScroll();InvalidateRect(h,nullptr,FALSE);return 0;}
    case WM_RBUTTONDOWN:{uint64_t a,b,n;if(!getBounds(a,b,n))return 0;HMENU menu=CreatePopupMenu(),st=CreatePopupMenu(),en=CreatePopupMenu(),both=CreatePopupMenu();AppendMenuW(st,MF_STRING,101,Tr(L"s086"));AppendMenuW(st,MF_STRING,102,Tr(L"s087"));AppendMenuW(en,MF_STRING,103,Tr(L"s086"));AppendMenuW(en,MF_STRING,104,Tr(L"s087"));AppendMenuW(both,MF_STRING,105,Tr(L"s086"));AppendMenuW(both,MF_STRING,106,Tr(L"s087"));AppendMenuW(menu,MF_POPUP,(UINT_PTR)st,Tr(L"s088"));AppendMenuW(menu,MF_POPUP,(UINT_PTR)en,Tr(L"s089"));AppendMenuW(menu,MF_POPUP,(UINT_PTR)both,Tr(L"s090"));POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(h,&p);UINT cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON|TPM_NONOTIFY,p.x,p.y,0,hwnd_,nullptr);DestroyMenu(menu);if(cmd==101)FindZeroCross(true,true);else if(cmd==102)FindZeroCross(true,false);else if(cmd==103)FindZeroCross(false,true);else if(cmd==104)FindZeroCross(false,false);else if(cmd==105){FindZeroCross(true,true,true);FindZeroCross(false,true,false);}else if(cmd==106){FindZeroCross(true,false,true);FindZeroCross(false,false,false);}InvalidateRect(h,nullptr,FALSE);return 0;}
    }
    return DefWindowProc(h,m,w,l);
}

void App::SelectKey(int idx,bool play,float velocity){if(!ValidPdx(idx))return;selected_=idx;ResetWaveView();int pitch=idx+3;prj_.internalOctave=std::clamp(pitch/12,0,8);UpdateControls();InvalidateRect(keys_,nullptr,FALSE);if(play)PreviewKey(idx,velocity);}
std::vector<float> App::ApplyPreviewGain(const std::vector<float>& in) const{
    float g=std::clamp(prj_.previewVolume,0,500)/100.0f;
    if(std::abs(g-1.0f)<0.0001f)return in;
    std::vector<float> out=in; for(auto& v:out)v=std::clamp(v*g,-1.0f,1.0f); return out;
}
void App::PreviewKey(int idx,float velocity){mousePlayer_.Stop();mouseKeyDisplayWave_.clear();const auto&s=prj_.banks[prj_.currentBank].slot[idx];if(s.wavPath.empty()){InvalidateRect(keys_,nullptr,FALSE);return;}WaveData w;if(!GetWave(s.wavPath,w))return;auto src=TrimForSlot(w,s);auto p=ProcessPcm(src,w.sampleRate,s.volume*velocity,s.transpose,s.pitchShift);auto en=EncodeTarget(p,prj_.format);auto de=DecodeTarget(en.data(),en.size(),prj_.format);mouseKeyDisplayWave_=de;auto pv=ApplyPreviewGain(de);mousePlayer_.Play(pv,15625);InvalidateRect(keys_,nullptr,FALSE);}
void App::StopPreview(){mousePlayer_.Stop();mouseKeyDisplayWave_.clear();InvalidateRect(keys_,nullptr,FALSE);}

void App::FinishListGesture(){
    if(!listHolding_)return;
    mousePlayer_.Stop(); listDisplayWave_.clear(); InvalidateListBody(list_);UpdateWindow(list_);
    int fileIndex=dragFile_;
    dragFile_=-1;
    listHolding_=false;
    if(GetCapture()==list_)ReleaseCapture();

    // Use the actual screen cursor position instead of WM_LBUTTONUP client
    // coordinates. This also works when the button-up was delivered elsewhere.
    POINT p{};GetCursorPos(&p);
    RECT kr{};GetWindowRect(keys_,&kr);
    if(fileIndex>=0&&fileIndex<(int)files_.size()&&PtInRect(&kr,p))
        AssignSelected(files_[fileIndex]);
}

LRESULT App::OnList(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_ERASEBKGND:
        // The item area is fully regenerated in WM_PAINT, so suppress the
        // separate erase pass that would otherwise flash between frames.
        return 1;
    case WM_PAINT:{
        PAINTSTRUCT ps{}; HDC dc=BeginPaint(h,&ps);
        RECT client{}; GetClientRect(h,&client);
        RECT body=ListBodyRect(h);
        const int cw=std::max(1,(int)(client.right-client.left));
        const int ch=std::max(1,(int)(client.bottom-client.top));

        HDC mem=CreateCompatibleDC(dc);
        HBITMAP bmp=CreateCompatibleBitmap(dc,cw,ch);
        HGDIOBJ old=SelectObject(mem,bmp);

        // Initialise the entire off-screen surface with the ListView's actual
        // background colour. WM_ERASEBKGND is intentionally suppressed for
        // flicker-free animation, so relying on PRF_ERASEBKGND here would leave
        // portions not painted by the native control as black bitmap contents.
        COLORREF listBk=ListView_GetBkColor(h);
        if(listBk==CLR_NONE)listBk=GetSysColor(COLOR_WINDOW);
        HBRUSH bkBrush=CreateSolidBrush(listBk);
        FillRect(mem,&client,bkBrush);
        DeleteObject(bkBrush);

        // Ask the native ListView implementation to render its client area to
        // the off-screen DC. The header is a separate child window and is not
        // copied back here, so its normal Windows drawing remains untouched.
        SendMessage(h,WM_PRINTCLIENT,(WPARAM)mem,PRF_CLIENT);

        if(body.bottom>body.top && !listDisplayWave_.empty() && mousePlayer_.IsPlaying()){
            DrawWaveOverlay(mem,body,listDisplayWave_);
            double frac=double(mousePlayer_.PositionSamples())/double(std::max<size_t>(1,listDisplayWave_.size()));
            DrawPlaybackBar(mem,body,frac);
        }

        // Transfer only the area below the header. This guarantees that every
        // animation frame replaces the previous waveform completely while the
        // header continues to be painted by its own native child window.
        if(body.bottom>body.top)
            BitBlt(dc,body.left,body.top,body.right-body.left,body.bottom-body.top,mem,body.left,body.top,SRCCOPY);

        SelectObject(mem,old); DeleteObject(bmp); DeleteDC(mem);
        EndPaint(h,&ps); return 0;
    }
    case WM_LBUTTONDOWN:{
        LVHITTESTINFO hi{};hi.pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};int i=ListView_HitTest(h,&hi);
        if(i>=0&&i<(int)files_.size()){
            // Do not enter the ListView's internal click/drag tracking loop here.
            // Select the row ourselves, start audio immediately on button-down,
            // and keep capture until the physical button is released.
            SetFocus(h);
            bool shift=(GetKeyState(VK_SHIFT)&0x8000)!=0;
            bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
            int anchor=ListView_GetSelectionMark(h);
            if(shift && anchor>=0){
                if(!ctrl)ListView_SetItemState(h,-1,0,LVIS_SELECTED|LVIS_FOCUSED);
                int lo=std::min(anchor,i),hi=std::max(anchor,i);
                for(int r=lo;r<=hi;++r)ListView_SetItemState(h,r,LVIS_SELECTED,LVIS_SELECTED);
                ListView_SetItemState(h,i,LVIS_FOCUSED,LVIS_FOCUSED);
            }else if(ctrl){
                UINT st=ListView_GetItemState(h,i,LVIS_SELECTED);
                ListView_SetItemState(h,i,(st&LVIS_SELECTED)?0:LVIS_SELECTED,LVIS_SELECTED);
                ListView_SetItemState(h,i,LVIS_FOCUSED,LVIS_FOCUSED);
                ListView_SetSelectionMark(h,i);
            }else{
                ListView_SetItemState(h,-1,0,LVIS_SELECTED|LVIS_FOCUSED);
                ListView_SetItemState(h,i,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
                ListView_SetSelectionMark(h,i);
            }
            ListView_EnsureVisible(h,i,FALSE);
            dragFile_=i; listHolding_=true;
            mousePlayer_.Stop(); listDisplayWave_.clear(); WaveData wd; if(GetWave(files_[i],wd)){listDisplayWave_=wd.mono;listDisplayRate_=wd.sampleRate;auto pv=ApplyPreviewGain(wd.mono);mousePlayer_.Play(pv,wd.sampleRate);InvalidateListBody(list_);}
            SetCapture(h);
            return 0;
        }
        return CallWindowProc(gOldList,h,m,w,l);
    }
    case WM_LBUTTONUP:
        FinishListGesture(); return 0;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        if(listHolding_ && !(GetAsyncKeyState(VK_LBUTTON)&0x8000)) FinishListGesture();
        break;
    }
    return CallWindowProc(gOldList,h,m,w,l);
}

void App::UpdateControls(){if(prj_.banks.empty())prj_.banks.resize(1);prj_.currentBank=std::clamp(prj_.currentBank,0,(int)prj_.banks.size()-1);updating_=true;const auto&s=CurSlot();wchar_t b[512];int pitch=selected_+3;swprintf_s(b,L"Bank %03d / PDX %02d / o%d%s",prj_.currentBank,selected_,pitch/12,L"");std::wstring t=b;if(!s.wavPath.empty())t+=L"  "+BaseName(s.wavPath);SetWindowTextW(slotLabel_,t.c_str());SendMessage(volTrack_,TBM_SETPOS,TRUE,(LPARAM)std::lround(s.volume*100));swprintf_s(b,L"%d",(int)std::lround(s.volume*100));SetWindowTextW(volEdit_,b);SendMessage(trTrack_,TBM_SETPOS,TRUE,s.transpose);swprintf_s(b,L"%+d",s.transpose);SetWindowTextW(trEdit_,b);CheckDlgButton(hwnd_,ID_STRETCH,s.pitchShift?BST_CHECKED:BST_UNCHECKED);SendMessage(bankCombo_,CB_SETCURSEL,prj_.currentBank,0);if(previewTrack_){SendMessage(previewTrack_,TBM_SETPOS,TRUE,prj_.previewVolume);swprintf_s(b,Tr(L"s026"),prj_.previewVolume);SetWindowTextW(previewLabel_,b);}updating_=false;if(waveEdit_)InvalidateRect(waveEdit_,nullptr,FALSE);}
void App::ReadControls(){if(updating_||!volEdit_||!trEdit_)return;wchar_t b[64]{};GetWindowTextW(volEdit_,b,64);int v=_wtoi(b);GetWindowTextW(trEdit_,b,64);int t=_wtoi(b);auto&s=CurSlot();s.volume=std::clamp(v,0,200)/100.f;s.transpose=std::clamp(t,-24,24);s.pitchShift=IsDlgButtonChecked(hwnd_,ID_STRETCH)==BST_CHECKED;SendMessage(volTrack_,TBM_SETPOS,TRUE,(LPARAM)std::lround(s.volume*100));SendMessage(trTrack_,TBM_SETPOS,TRUE,s.transpose);s.importedRaw=false;MarkDirty();}

void App::RefreshBankCombo(){if(!bankCombo_)return;updating_=true;SendMessage(bankCombo_,CB_RESETCONTENT,0,0);for(int b=0;b<256;++b){wchar_t s[16];swprintf_s(s,L"%03d",b);SendMessage(bankCombo_,CB_ADDSTRING,0,(LPARAM)s);}SendMessage(bankCombo_,CB_SETCURSEL,prj_.currentBank,0);updating_=false;}
void App::SwitchBank(int bank,bool fromProgramChange){if(bank<0||bank>255)return;if((int)prj_.banks.size()<=bank)prj_.banks.resize((size_t)bank+1);prj_.currentBank=bank;UpdateControls();InvalidateRect(keys_,nullptr,FALSE);MarkDirty();Log((fromProgramChange?L"MIDI Program Change: Bank ":Tr(L"s048"))+std::to_wstring(bank));}

bool App::GetWave(const std::wstring&p,WaveData&o){auto it=cache_.find(p);if(it!=cache_.end()){o=it->second;return true;}std::wstring e;if(!LoadWave(p,o,e)){Log(Tr(L"s049")+BaseName(p)+L" : "+e);return false;}cache_[p]=o;return true;}
void App::AddWavePath(const std::wstring&p){for(auto&x:files_)if(!_wcsicmp(x.c_str(),p.c_str()))return;WaveData w;std::wstring e;if(!LoadWave(p,w,e)){Log(Tr(L"s050")+BaseName(p)+L" : "+e);return;}files_.push_back(p);cache_[p]=w;AddToList(p,w);Log(Tr(L"s051")+BaseName(p));}
void App::AddFolder(const std::wstring&p){try{for(auto&x:fs::recursive_directory_iterator(p))if(x.is_regular_file()&&!_wcsicmp(x.path().extension().c_str(),L".wav"))AddWavePath(x.path().wstring());}catch(...){Log(Tr(L"s052")+p);}}
void App::AddToList(const std::wstring&p,const WaveData&w){
    int i=ListView_GetItemCount(list_);
    std::wstring dir;
    try{
        fs::path parent=fs::absolute(fs::path(p)).parent_path().lexically_normal();
        fs::path base=fs::current_path().lexically_normal();
        fs::path rel=parent.lexically_relative(base);
        if(!rel.empty() && rel.native().rfind(L"..",0)!=0) dir=rel.empty()?L".":rel.wstring();
        else if(parent==base) dir=L".";
        else dir=parent.wstring();
    }catch(...){dir=fs::path(p).parent_path().wstring();}
    if(dir.empty())dir=L".";
    LVITEMW it{LVIF_TEXT};it.iItem=i;it.pszText=(LPWSTR)dir.c_str();ListView_InsertItem(list_,&it);
    std::wstring bn=BaseName(p);ListView_SetItemText(list_,i,1,(LPWSTR)bn.c_str());
    try{auto ft=fs::last_write_time(p);auto st=std::chrono::time_point_cast<std::chrono::system_clock::duration>(ft-fs::file_time_type::clock::now()+std::chrono::system_clock::now());time_t tt=std::chrono::system_clock::to_time_t(st);tm tmv{};localtime_s(&tmv,&tt);wchar_t b[64];wcsftime(b,64,L"%Y/%m/%d %H:%M",&tmv);ListView_SetItemText(list_,i,2,b);}catch(...){ }
    wchar_t b[64]{};
    try{uintmax_t bytes=fs::file_size(p);if(bytes>=1024ull*1024ull)swprintf_s(b,L"%.2f MB",double(bytes)/(1024.0*1024.0));else if(bytes>=1024ull)swprintf_s(b,L"%.1f KB",double(bytes)/1024.0);else swprintf_s(b,L"%llu B",(unsigned long long)bytes);ListView_SetItemText(list_,i,3,b);}catch(...){ }
    double sec=w.sampleRate?double(w.mono.size())/double(w.sampleRate):0.0;int mins=(int)(sec/60.0);double rem=sec-mins*60.0;swprintf_s(b,L"%d:%06.3f",mins,rem);ListView_SetItemText(list_,i,4,b);
    swprintf_s(b,L"%u Hz",w.sampleRate);ListView_SetItemText(list_,i,5,b);swprintf_s(b,L"%u bit",w.bits);ListView_SetItemText(list_,i,6,b);
    std::wstring ch=(w.channels==1?Tr(L"s053"):(w.channels==2?Tr(L"s054"):std::to_wstring(w.channels)+L" ch"));ListView_SetItemText(list_,i,7,(LPWSTR)ch.c_str());
    ListView_SetItemText(list_,i,8,(LPWSTR)w.formatName.c_str());
}
void App::RebuildFileList(){ListView_DeleteAllItems(list_);for(auto&p:files_){WaveData w;if(GetWave(p,w))AddToList(p,w);}}
void App::SortList(int col){
    static bool asc[9]={true,true,true,true,true,true,true,true,true};if(col<0||col>=9)return;asc[col]=!asc[col];
    std::stable_sort(files_.begin(),files_.end(),[&](auto&a,auto&b){WaveData wa,wb;GetWave(a,wa);GetWave(b,wb);int c=0;
        if(col==0)c=_wcsicmp(fs::path(a).parent_path().wstring().c_str(),fs::path(b).parent_path().wstring().c_str());
        else if(col==1)c=_wcsicmp(BaseName(a).c_str(),BaseName(b).c_str());
        else if(col==2){auto aa=fs::last_write_time(a),bb=fs::last_write_time(b);c=aa<bb?-1:aa>bb?1:0;}
        else if(col==3){uintmax_t aa=0,bb=0;try{aa=fs::file_size(a);}catch(...){ }try{bb=fs::file_size(b);}catch(...){ }c=aa<bb?-1:aa>bb?1:0;}
        else if(col==4){double aa=wa.sampleRate?double(wa.mono.size())/wa.sampleRate:0.0,bb=wb.sampleRate?double(wb.mono.size())/wb.sampleRate:0.0;c=aa<bb?-1:aa>bb?1:0;}
        else if(col==5)c=wa.sampleRate<wb.sampleRate?-1:wa.sampleRate>wb.sampleRate?1:0;
        else if(col==6)c=wa.bits<wb.bits?-1:wa.bits>wb.bits?1:0;
        else if(col==7)c=wa.channels<wb.channels?-1:wa.channels>wb.channels?1:0;
        else c=_wcsicmp(wa.formatName.c_str(),wb.formatName.c_str());return asc[col]?c<0:c>0;});RebuildFileList();
}
void App::AddDropped(HDROP d){UINT n=DragQueryFileW(d,0xFFFFFFFF,nullptr,0);for(UINT i=0;i<n;i++){wchar_t p[32768];DragQueryFileW(d,i,p,32768);fs::path fp(p);if(fs::is_directory(fp))AddFolder(p);else if(!_wcsicmp(fp.extension().c_str(),L".wav"))AddWavePath(p);else if(!_wcsicmp(fp.extension().c_str(),L".pdx"))ImportPdx(p);else if(!_wcsicmp(fp.extension().c_str(),L".pdxedit"))LoadProjectFile(p,true);else Log(Tr(L"s055")+BaseName(p));}DragFinish(d);}
void App::Log(const std::wstring&s){if(!log_)return;int len=GetWindowTextLengthW(log_);SendMessage(log_,EM_SETSEL,len,len);std::wstring t=s+L"\r\n";SendMessage(log_,EM_REPLACESEL,FALSE,(LPARAM)t.c_str());SendMessage(log_,EM_SCROLLCARET,0,0);}
void App::MarkDirty(){if(restoringHistory_)return;dirty_=true;changedAt_=TickMs();}
void App::CaptureUiState(){if(!hwnd_)return;WINDOWPLACEMENT wp{sizeof(wp)};if(GetWindowPlacement(hwnd_,&wp)&&wp.showCmd!=SW_SHOWMINIMIZED){RECT r=wp.rcNormalPosition;prj_.windowX=r.left;prj_.windowY=r.top;prj_.windowW=r.right-r.left;prj_.windowH=r.bottom-r.top;}if(list_)for(int i=0;i<9;++i)prj_.listWidths[i]=ListView_GetColumnWidth(list_,i);}
void App::ApplyUiState(){updating_=true;if(prj_.windowW>=640&&prj_.windowH>=480&&prj_.windowX!=CW_USEDEFAULT)SetWindowPos(hwnd_,nullptr,prj_.windowX,prj_.windowY,prj_.windowW,prj_.windowH,SWP_NOZORDER|SWP_NOACTIVATE);for(int i=0;i<9;++i)ListView_SetColumnWidth(list_,i,prj_.listWidths[i]);updating_=false;}
std::wstring App::CurrentProjectPath()const{fs::path dir=fs::path(projectPath_).has_parent_path()?fs::path(projectPath_).parent_path():fs::current_path();std::wstring n=prj_.pdxName.empty()?L"pcm":prj_.pdxName;return(dir/(n+L".pdxedit")).wstring();}
bool App::SaveCurrentNow(){CaptureUiState();std::wstring e,pp=CurrentProjectPath();if(SaveProject(pp,prj_,e)){projectPath_=pp;dirty_=false;std::ofstream lf(LastProjectMarkerPath(),std::ios::binary);lf<<WToUtf8(projectPath_);Log(Tr(L"s056")+BaseName(pp));return true;}Log(Tr(L"s057")+e);return false;}
void App::SaveAuto(){if(dirty_&&TickMs()-changedAt_>=2000)SaveCurrentNow();}

void App::LoadProjectFile(const std::wstring&p,bool ask){
    fs::path projectAbs;
    try{ projectAbs=fs::absolute(fs::path(p)); }catch(...){ projectAbs=fs::path(p); }
    if(ask){
        std::wstring q=std::wstring(Tr(L"s058"))+BaseName(projectAbs.wstring())+Tr(L"s059");
        if(MessageBoxW(hwnd_,q.c_str(),Tr(L"s060"),MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
    }
    ProjectSetting s;std::wstring e;
    if(!LoadProject(projectAbs.wstring(),s,e)){MessageBoxW(hwnd_,e.c_str(),Tr(L"s061"),MB_OK|MB_ICONERROR);return;}
    // The folder containing the project is the working/current folder.
    try{ if(projectAbs.has_parent_path()) fs::current_path(projectAbs.parent_path()); }catch(...){ }
    if(!s.languageCode.empty()){int li=LanguageIndexByCode(s.languageCode);if(li>=0)s.language=li;}
    if(s.language<0||s.language>=(int)gLanguagePacks.size())s.language=prj_.language;
    if(s.languageCode.empty()&&s.language>=0&&s.language<(int)gLanguagePacks.size())s.languageCode=gLanguagePacks[s.language].code;
    prj_=std::move(s);SelectLanguageIndex(prj_.language);projectPath_=projectAbs.wstring();files_.clear();cache_.clear();ListView_DeleteAllItems(list_);
    for(auto&b:prj_.banks)for(auto&sl:b.slot)if(!sl.wavPath.empty()&&fs::exists(sl.wavPath))AddWavePath(sl.wavPath);
    if(!prj_.sourcePdx.empty()&&fs::exists(prj_.sourcePdx)){ImportedPdx px;std::wstring pe;if(ReadPdx(prj_.sourcePdx,px,pe)){for(size_t b=0;b<prj_.banks.size()&&b<px.table.size();++b)for(int i=0;i<96;++i)if(prj_.banks[b].slot[i].importedRaw&&px.table[b][i].length){auto en=px.table[b][i];prj_.banks[b].slot[i].raw.assign(px.bytes.begin()+en.offset,px.bytes.begin()+en.offset+en.length);}}}
    undo_.clear();redo_.clear();updating_=true;SetWindowTextW(name_,prj_.pdxName.c_str());SendMessage(fmt_,CB_SETCURSEL,(int)prj_.format,0);updating_=false;RefreshBankCombo();selected_=0;ApplyUiState();ApplyLanguage();UpdateControls();InvalidateRect(keys_,nullptr,FALSE);
    std::ofstream lf(LastProjectMarkerPath(),std::ios::binary);lf<<WToUtf8(projectPath_);
    Log(Tr(L"s062")+BaseName(projectAbs.wstring()));
}

void App::ImportPdx(const std::wstring&p){
    fs::path pdxAbs;try{pdxAbs=fs::absolute(fs::path(p));}catch(...){pdxAbs=fs::path(p);}
    std::wstring q=std::wstring(Tr(L"s058"))+BaseName(pdxAbs.wstring())+Tr(L"s059");
    if(MessageBoxW(hwnd_,q.c_str(),Tr(L"s063"),MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
    ImportedPdx x;std::wstring e;if(!ReadPdx(pdxAbs.wstring(),x,e)){MessageBoxW(hwnd_,e.c_str(),Tr(L"s064"),MB_OK|MB_ICONERROR);return;}
    auto stem=pdxAbs.stem().wstring();fs::path workDir=AppDirectory();
    try{fs::current_path(workDir);}catch(...){ }
    auto folder=(workDir/stem).wstring();std::vector<std::array<std::wstring,96>> paths;
    if(!ExtractPdxAllBanks(pdxAbs.wstring(),x,folder,paths,e)){MessageBoxW(hwnd_,e.c_str(),Tr(L"s065"),MB_OK|MB_ICONERROR);return;}
    int keepLanguage=prj_.language;std::wstring keepLanguageCode=prj_.languageCode;prj_=ProjectSetting{};prj_.language=keepLanguage;prj_.languageCode=keepLanguageCode;SelectLanguageIndex(prj_.language);prj_.pdxName=stem;prj_.format=x.guessed;prj_.sourceFormat=x.guessed;prj_.sourcePdx=pdxAbs.wstring();prj_.banks.clear();prj_.banks.resize(std::max(1,x.banks));prj_.currentBank=0;files_.clear();cache_.clear();ListView_DeleteAllItems(list_);
    for(int b=0;b<x.banks;++b)for(int i=0;i<96;++i)if(!paths[b][i].empty()){auto&s=prj_.banks[b].slot[i];s.wavPath=paths[b][i];s.importedRaw=true;auto en=x.table[b][i];s.raw.assign(x.bytes.begin()+en.offset,x.bytes.begin()+en.offset+en.length);AddWavePath(paths[b][i]);}
    projectPath_=(workDir/(stem+L".pdxedit")).wstring();updating_=true;SetWindowTextW(name_,prj_.pdxName.c_str());SendMessage(fmt_,CB_SETCURSEL,(int)prj_.format,0);updating_=false;RefreshBankCombo();selected_=0;UpdateControls();InvalidateRect(keys_,nullptr,FALSE);undo_.clear();redo_.clear();dirty_=true;SaveCurrentNow();Log(Tr(L"s066")+BaseName(pdxAbs.wstring())+L" / bank="+std::to_wstring(x.banks));
}

void App::Convert(){
    ReadControls();
    int lastDataBank=-1;
    for(int b=0;b<(int)prj_.banks.size()&&b<256;++b){
        for(const auto& sl:prj_.banks[b].slot){if(!sl.wavPath.empty()||!sl.raw.empty()){lastDataBank=b;break;}}
    }
    if(lastDataBank<0){MessageBoxW(hwnd_,Tr(L"s067"),Tr(L"s068"),MB_OK|MB_ICONINFORMATION);return;}
    // Preserve every bank from 000 through the last bank that contains data.
    // This keeps intentionally empty leading/intermediate banks in their original
    // positions; only trailing empty banks are omitted.
    std::vector<std::array<std::vector<uint8_t>,96>> out((size_t)lastDataBank+1);
    int used=0;
    for(int bankNo=0;bankNo<=lastDataBank;++bankNo){
        if(bankNo>=(int)prj_.banks.size())continue;
        for(int i=0;i<96;++i){auto&s=prj_.banks[bankNo].slot[i];if(s.wavPath.empty())continue;
            if(s.importedRaw&&prj_.format==prj_.sourceFormat&&s.volume==1.f&&s.transpose==0&&!s.pitchShift&&!s.raw.empty()){out[bankNo][i]=s.raw;++used;continue;}
            WaveData wd;if(!GetWave(s.wavPath,wd))continue;auto src=TrimForSlot(wd,s);auto pcm=ProcessPcm(src,wd.sampleRate,s.volume,s.transpose,s.pitchShift);out[bankNo][i]=EncodeTarget(pcm,prj_.format);++used;
        }
    }
    std::wstring name=prj_.pdxName.empty()?L"pcm":prj_.pdxName;if(fs::path(name).extension().empty())name+=L".pdx";fs::path base=projectPath_.empty()?fs::current_path():fs::path(projectPath_).parent_path();std::wstring path=(base/name).wstring(),e;std::vector<std::wstring>lg;
    if(WriteExPdxMulti(path,out,e,&lg)){for(auto&x:lg)Log(x);Log(Tr(L"s069")+path+L" ("+std::to_wstring(out.size())+L" bank / "+std::to_wstring(used)+Tr(L"s070"));SaveCurrentNow();MessageBoxW(hwnd_,(std::wstring(Tr(L"s071"))+path).c_str(),Tr(L"s072"),MB_OK|MB_ICONINFORMATION);}else{Log(Tr(L"s073")+e);MessageBoxW(hwnd_,e.c_str(),Tr(L"s074"),MB_OK|MB_ICONERROR);}
}
void App::RefreshMidi(){SendMessage(midiCombo_,CB_RESETCONTENT,0,0);UINT n=midiInGetNumDevs();for(UINT i=0;i<n;++i){MIDIINCAPSW c{};if(midiInGetDevCapsW(i,&c,sizeof(c))==MMSYSERR_NOERROR)SendMessage(midiCombo_,CB_ADDSTRING,0,(LPARAM)c.szPname);}if(n)SendMessage(midiCombo_,CB_SETCURSEL,0,0);}
void App::CloseMidi(){for(auto&p:midiPlayers_)p.Stop();if(midiIn_){midiInStop(midiIn_);midiInReset(midiIn_);midiInClose(midiIn_);midiIn_=nullptr;}midiDown_.clear();for(auto&v:midiDisplayWave_)v.clear();midiPressure_.fill(0.f);channelPressure_=1.f;InvalidateRect(keys_,nullptr,FALSE);}
void App::OpenMidi(){CloseMidi();if(IsDlgButtonChecked(hwnd_,ID_MIDITOGGLE)!=BST_CHECKED)return;int d=(int)SendMessage(midiCombo_,CB_GETCURSEL,0,0);if(d<0)return;if(midiInOpen(&midiIn_,d,(DWORD_PTR)MidiCb,0,CALLBACK_FUNCTION)==MMSYSERR_NOERROR){midiInStart(midiIn_);Log(Tr(L"s075"));}else{midiIn_=nullptr;CheckDlgButton(hwnd_,ID_MIDITOGGLE,BST_UNCHECKED);Log(Tr(L"s076"));}}
void App::HandleMidi(DWORD m){
    int st=m&0xF0,d1=(m>>8)&0x7F,d2=(m>>16)&0x7F;
    if(st==0xC0){SwitchBank(d1,true);return;}

    // Aftertouch is intentionally ignored. The current waveOut preview path uses
    // one-shot buffers, so changing gain would require restarting the sample.
    // That behavior is musically incorrect for aftertouch; keep the held note
    // continuous instead of retriggering it.
    if(st==0xA0 || st==0xD0)return;

    int pitch=d1-12,idx=pitch-3;
    if(!ValidPdx(idx))return;
    if(st==0x90&&d2>0){
        midiDown_.insert(idx);
        selected_=*midiDown_.rbegin();prj_.internalOctave=std::clamp((selected_+3)/12,0,8);UpdateControls();
        const auto& sl=prj_.banks[prj_.currentBank].slot[idx];
        if(!sl.wavPath.empty()){
            WaveData wd;if(GetWave(sl.wavPath,wd)){
                float velocity=d2/127.f;
                auto src=TrimForSlot(wd,sl);auto pc=ProcessPcm(src,wd.sampleRate,sl.volume*velocity,sl.transpose,sl.pitchShift);
                auto en=EncodeTarget(pc,prj_.format);auto de=DecodeTarget(en.data(),en.size(),prj_.format);midiDisplayWave_[idx]=de;auto pv=ApplyPreviewGain(de);
                midiPlayers_[idx].Play(pv,15625);
            }
        }
    }else if(st==0x80||(st==0x90&&d2==0)){
        midiDown_.erase(idx);midiPlayers_[idx].Stop();midiDisplayWave_[idx].clear();
        if(!midiDown_.empty()){selected_=*midiDown_.rbegin();prj_.internalOctave=std::clamp((selected_+3)/12,0,8);UpdateControls();}
    }
    InvalidateRect(keys_,nullptr,FALSE);
}

void App::KeyboardNote(int sem,bool down){
    if(down){if(keyDownSemis_.count(sem))return;keyDownSemis_.insert(sem);int idx=PitchToPdx(prj_.internalOctave,sem);if(!ValidPdx(idx)){InvalidateRect(keys_,nullptr,FALSE);return;}selected_=idx;UpdateControls();const auto&s=CurSlot();if(!s.wavPath.empty()){WaveData wd;if(GetWave(s.wavPath,wd)){auto src=TrimForSlot(wd,s);auto pc=ProcessPcm(src,wd.sampleRate,s.volume,s.transpose,s.pitchShift);auto en=EncodeTarget(pc,prj_.format);auto de=DecodeTarget(en.data(),en.size(),prj_.format);keyDisplayWave_[sem]=de;auto pv=ApplyPreviewGain(de);keyPlayers_[sem].Play(pv,15625);}}}
    else{if(!keyDownSemis_.erase(sem))return;keyPlayers_[sem].Stop();keyDisplayWave_[sem].clear();}
    InvalidateRect(keys_,nullptr,FALSE);
}
bool App::FocusIsText()const{HWND f=GetFocus();return f==name_||f==volEdit_||f==trEdit_||f==log_;}

void App::AssignSelected(const std::wstring& path){
    if(path.empty() || !fs::exists(path)) return;
    WaveData wd;
    if(!GetWave(path,wd)) return;
    if(prj_.currentBank<0 || prj_.currentBank>255 || !ValidPdx(selected_)) return;
    if((int)prj_.banks.size()<=prj_.currentBank) prj_.banks.resize((size_t)prj_.currentBank+1);
    PushUndo();
    auto& s=CurSlot();
    s.wavPath=path;
    s.importedRaw=false;
    s.raw.clear();
    s.trimStart=0; s.trimEnd=0; ResetWaveView();
    MarkDirty();
    UpdateControls();
    InvalidateRect(keys_,nullptr,FALSE);
    Log(Tr(L"s077")+std::to_wstring(prj_.currentBank)+L" PDX "+
        (selected_<10?L"0":L"")+std::to_wstring(selected_)+L" <- "+BaseName(path));
}

void App::DeleteFocused(){
    HWND f=GetFocus();
    if(f==list_ || IsChild(list_,f)){
        std::vector<int> rows;
        for(int i=-1;(i=ListView_GetNextItem(list_,i,LVNI_SELECTED))!=-1;) rows.push_back(i);
        if(rows.empty()) return;
        PushUndo();
        std::vector<std::wstring> removed;
        for(int i:rows) if(i>=0 && i<(int)files_.size()) removed.push_back(files_[i]);
        int cleared=0;
        for(const auto& path:removed){
            for(auto& bank:prj_.banks) for(auto& sl:bank.slot){
                if(!sl.wavPath.empty() && !_wcsicmp(sl.wavPath.c_str(),path.c_str())){
                    sl=SlotSetting{};
                    ++cleared;
                }
            }
            cache_.erase(path);
            files_.erase(std::remove_if(files_.begin(),files_.end(),[&](const std::wstring& x){return !_wcsicmp(x.c_str(),path.c_str());}),files_.end());
        }
        RebuildFileList();
        UpdateControls();
        InvalidateRect(keys_,nullptr,FALSE);
        MarkDirty();
        Log(Tr(L"s078")+std::to_wstring(removed.size())+Tr(L"s079")+std::to_wstring(cleared)+Tr(L"s080"));
        return;
    }
    if(!ValidPdx(selected_)) return;
    auto& sl=CurSlot();
    if(sl.wavPath.empty() && sl.raw.empty()) return;
    PushUndo();
    sl=SlotSetting{};
    mousePlayer_.Stop();
    UpdateControls();
    InvalidateRect(keys_,nullptr,FALSE);
    MarkDirty();
    Log(Tr(L"s081")+std::to_wstring(prj_.currentBank)+L" PDX "+
        (selected_<10?L"0":L"")+std::to_wstring(selected_));
}

App::HistoryState App::Snapshot() const{return HistoryState{prj_,files_};}
void App::PushUndo(){if(restoringHistory_)return;undo_.push_back(Snapshot());if(undo_.size()>100)undo_.erase(undo_.begin());redo_.clear();}
void App::RestoreState(const HistoryState&s){restoringHistory_=true;prj_=s.prj;files_=s.files;cache_.clear();RebuildFileList();RefreshBankCombo();updating_=true;SetWindowTextW(name_,prj_.pdxName.c_str());SendMessage(fmt_,CB_SETCURSEL,(int)prj_.format,0);updating_=false;selected_=std::clamp(selected_,0,95);ResetWaveView();UpdateControls();InvalidateRect(keys_,nullptr,FALSE);restoringHistory_=false;MarkDirty();}
void App::Undo(){if(undo_.empty())return;redo_.push_back(Snapshot());auto s=undo_.back();undo_.pop_back();RestoreState(s);dirty_=true;changedAt_=TickMs();Log(Tr(L"s091"));}
void App::Redo(){if(redo_.empty())return;undo_.push_back(Snapshot());auto s=redo_.back();redo_.pop_back();RestoreState(s);dirty_=true;changedAt_=TickMs();Log(Tr(L"s092"));}
void App::BeginControlEdit(){if(!pendingControlEdit_)pendingControlEdit_=Snapshot();}
void App::EndControlEdit(){if(!pendingControlEdit_)return;undo_.push_back(*pendingControlEdit_);if(undo_.size()>100)undo_.erase(undo_.begin());redo_.clear();pendingControlEdit_.reset();Log(Tr(L"s025")+std::to_wstring(prj_.currentBank)+L" PDX "+std::to_wstring(selected_));}
