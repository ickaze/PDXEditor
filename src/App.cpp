#include "App.h"
#include <windowsx.h>

App* App::self=nullptr;
static WNDPROC gOldList=nullptr;
static const wchar_t* kClass=L"PDXEditorMain";
static const wchar_t* kKeys=L"PDXEditorKeys";

enum {
    ID_NAME=100, ID_FMT, ID_LOAD, ID_CONVERT, ID_BANK,
    ID_MIDICOMBO, ID_MIDITOGGLE, ID_LIST, ID_LOG,
    ID_VOLTR, ID_VOLEDIT, ID_TRTR, ID_TREDIT, ID_STRETCH, ID_PREVIEWTR
};

static int PitchToPdx(int octave,int semi){ return octave*12+semi-3; }
static bool ValidPdx(int idx){ return idx>=0 && idx<96; }
static bool IsBlackSemi(int s){ return s==1||s==3||s==6||s==8||s==10; }

int App::Run(HINSTANCE h){
    self=this; inst_=h;
    INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_LISTVIEW_CLASSES|ICC_BAR_CLASSES}; InitCommonControlsEx(&ic);
    WNDCLASSW wc{}; wc.hInstance=h; wc.lpfnWndProc=WndProc; wc.lpszClassName=kClass; wc.hCursor=LoadCursor(nullptr,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); RegisterClassW(&wc);
    WNDCLASSW kc=wc; kc.lpfnWndProc=KeysProc; kc.lpszClassName=kKeys; kc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH); RegisterClassW(&kc);
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
void CALLBACK App::MidiCb(HMIDIIN,UINT msg,DWORD_PTR,DWORD_PTR p1,DWORD_PTR){ if(self&&msg==MIM_DATA)PostMessage(self->hwnd_,WM_APP+1,(WPARAM)p1,0); }

SlotSetting& App::CurSlot(){ return prj_.banks[prj_.currentBank].slot[selected_]; }
const SlotSetting& App::CurSlot() const { return prj_.banks[prj_.currentBank].slot[selected_]; }

LRESULT App::OnMsg(UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE:
        CreateUI(); DragAcceptFiles(hwnd_,TRUE); SetTimer(hwnd_,1,250,nullptr); return 0;
    case WM_MOVE:
        if(!updating_){CaptureUiState(); MarkDirty();} return 0;
    case WM_SIZE:
        Layout(); if(!updating_ && w!=SIZE_MINIMIZED){CaptureUiState(); MarkDirty();} return 0;
    case WM_DROPFILES: AddDropped((HDROP)w); return 0;
    case WM_TIMER:
        // ListView can alter mouse capture internally. If button-up was not
        // delivered to the subclass, finish the preview/drop here as a fallback.
        if(listHolding_ && !(GetAsyncKeyState(VK_LBUTTON)&0x8000)) FinishListGesture();
        SaveAuto(); return 0;
    case WM_APP+1: HandleMidi((DWORD)w); return 0;
    case WM_COMMAND:{
        int id=LOWORD(w), code=HIWORD(w);
        if(id==ID_CONVERT) Convert();
        else if(id==ID_LOAD&&code==BN_CLICKED){
            OPENFILENAMEW o{sizeof(o)}; wchar_t b[MAX_PATH]{}; o.hwndOwner=hwnd_; o.lpstrFile=b; o.nMaxFile=MAX_PATH;
            o.lpstrFilter=L"PDX Editor Project (*.pdxedit)\0*.pdxedit\0All Files\0*.*\0"; o.Flags=OFN_FILEMUSTEXIST;
            if(GetOpenFileNameW(&o))LoadProjectFile(b,false);
        }
        else if(id==ID_FMT&&code==CBN_SELCHANGE&&!updating_){ PushUndo(); prj_.format=(PcmFormat)SendMessage(fmt_,CB_GETCURSEL,0,0); for(auto& b:prj_.banks)for(auto& s:b.slot)s.importedRaw=false; MarkDirty(); }
        else if(id==ID_NAME&&code==EN_CHANGE&&!updating_){ wchar_t b[260]{}; GetWindowTextW(name_,b,260); prj_.pdxName=b; MarkDirty(); }
        else if(id==ID_BANK&&code==CBN_SELCHANGE&&!updating_) SwitchBank((int)SendMessage(bankCombo_,CB_GETCURSEL,0,0));
        else if(id==ID_MIDITOGGLE&&code==BN_CLICKED)OpenMidi();
        else if(id==ID_MIDICOMBO&&code==CBN_SELCHANGE&&IsDlgButtonChecked(hwnd_,ID_MIDITOGGLE))OpenMidi();
        else if((id==ID_VOLEDIT||id==ID_TREDIT)&&code==EN_SETFOCUS)BeginControlEdit();
        else if((id==ID_VOLEDIT||id==ID_TREDIT)&&code==EN_CHANGE)ReadControls();
        else if((id==ID_VOLEDIT||id==ID_TREDIT)&&code==EN_KILLFOCUS)EndControlEdit();
        else if(id==ID_STRETCH&&code==BN_CLICKED){PushUndo(); ReadControls(); Log(L"鍵盤設定を変更: Bank "+std::to_wstring(prj_.currentBank)+L" PDX "+std::to_wstring(selected_));}
        return 0;
    }
    case WM_HSCROLL:
        if((HWND)l==previewTrack_){
            int v=(int)SendMessage(previewTrack_,TBM_GETPOS,0,0);
            prj_.previewVolume=std::clamp(v,0,500);
            wchar_t b[64]; swprintf_s(b,L"試聴音量 %d%%",prj_.previewVolume); SetWindowTextW(previewLabel_,b);
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
            if(down && (GetKeyState(VK_CONTROL)&0x8000) && (w=='Z')){
                if(GetKeyState(VK_SHIFT)&0x8000)Redo(); else Undo(); return 0;
            }
            if(down && w==VK_DELETE){DeleteFocused();return 0;}
            const wchar_t* map=L"AWSEDFTGYHUJ"; int sem=-1;
            for(int i=0;i<12;++i)if((wchar_t)w==map[i]){sem=i;break;}
            if(sem>=0){KeyboardNote(sem,down);return 0;}
            if(down && !(l&(1LL<<30)) && w==VK_UP){prj_.internalOctave=std::max(0,prj_.internalOctave-1);MarkDirty();InvalidateRect(keys_,nullptr,FALSE);return 0;}
            if(down && !(l&(1LL<<30)) && w==VK_DOWN){prj_.internalOctave=std::min(8,prj_.internalOctave+1);MarkDirty();InvalidateRect(keys_,nullptr,FALSE);return 0;}
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
    C(0,L"STATIC",L"PDX名:",SS_LEFT,0); name_=C(WS_EX_CLIENTEDGE,L"EDIT",L"pcm",ES_AUTOHSCROLL,ID_NAME);
    C(0,L"STATIC",L"形式:",SS_LEFT,0); fmt_=C(0,WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL,ID_FMT);
    for(auto*s:{L"ADPCM",L"P16 (16bit PCM)",L"P8 (8bit PCM)"})SendMessage(fmt_,CB_ADDSTRING,0,(LPARAM)s); SendMessage(fmt_,CB_SETCURSEL,0,0);
    C(0,L"BUTTON",L"設定を読み込む",BS_PUSHBUTTON,ID_LOAD); C(0,L"BUTTON",L"変換 / PDX出力",BS_DEFPUSHBUTTON,ID_CONVERT);
    C(0,L"STATIC",L"Bank:",SS_LEFT,0); bankCombo_=C(0,WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL,ID_BANK);
    midiToggle_=C(0,L"BUTTON",L"MIDI入力反映",BS_AUTOCHECKBOX,ID_MIDITOGGLE); midiCombo_=C(0,WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL,ID_MIDICOMBO);
    keys_=CreateWindowExW(WS_EX_CLIENTEDGE,kKeys,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,10,10,hwnd_,nullptr,inst_,nullptr);
    list_=C(WS_EX_CLIENTEDGE,WC_LISTVIEWW,L"",LVS_REPORT|LVS_SHOWSELALWAYS|LVS_SINGLESEL,ID_LIST);
    ListView_SetExtendedListViewStyle(list_,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_HEADERDRAGDROP);
    const wchar_t* cols[]={L"ファイル名",L"タイムスタンプ",L"周波数",L"ビット数",L"チャンネル",L"形式"};
    for(int i=0;i<6;++i){LVCOLUMNW c{LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM};c.pszText=(LPWSTR)cols[i];c.cx=prj_.listWidths[i];c.iSubItem=i;ListView_InsertColumn(list_,i,&c);} gOldList=(WNDPROC)SetWindowLongPtr(list_,GWLP_WNDPROC,(LONG_PTR)ListProc);
    log_=C(WS_EX_CLIENTEDGE,L"EDIT",L"",ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,ID_LOG);
    slotLabel_=C(0,L"STATIC",L"PDX 00",SS_LEFT,0); C(0,L"STATIC",L"音量 %",SS_LEFT,0);
    volTrack_=C(0,TRACKBAR_CLASSW,L"",TBS_HORZ,ID_VOLTR);SendMessage(volTrack_,TBM_SETRANGE,TRUE,MAKELONG(0,200));volEdit_=C(WS_EX_CLIENTEDGE,L"EDIT",L"100",ES_NUMBER,ID_VOLEDIT);
    C(0,L"STATIC",L"Transpose",SS_LEFT,0); trTrack_=C(0,TRACKBAR_CLASSW,L"",TBS_HORZ,ID_TRTR);SendMessage(trTrack_,TBM_SETRANGE,TRUE,MAKELONG(-24,24));trEdit_=C(WS_EX_CLIENTEDGE,L"EDIT",L"0",ES_AUTOHSCROLL,ID_TREDIT);
    stretch_=C(0,L"BUTTON",L"ピッチシフト",BS_AUTOCHECKBOX,ID_STRETCH);
    previewLabel_=C(0,L"STATIC",L"試聴音量 100%",SS_LEFT,0);
    previewTrack_=C(0,TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_AUTOTICKS,ID_PREVIEWTR); SendMessage(previewTrack_,TBM_SETRANGE,TRUE,MAKELONG(0,500)); SendMessage(previewTrack_,TBM_SETTICFREQ,50,0); SendMessage(previewTrack_,TBM_SETPOS,TRUE,100);
    RefreshMidi(); RefreshBankCombo(); selected_=0; UpdateControls();
    Log(L"起動しました。WAV/フォルダ/PDX/設定ファイルをドロップできます。");
    std::ifstream lf(fs::current_path()/L"PDXEditor.last",std::ios::binary);
    if(lf){std::string u;std::getline(lf,u);auto lp=Utf8ToW(u);if(!lp.empty()&&fs::exists(lp))LoadProjectFile(lp,false);}
}

void App::Layout(){
    if(!hwnd_||!keys_)return; RECT r{};GetClientRect(hwnd_,&r);int W=(int)r.right,H=(int)r.bottom;
    HWND h=GetWindow(hwnd_,GW_CHILD); int y=10;
    auto mv=[&](int x,int yy,int w,int hh){if(h){MoveWindow(h,x,yy,w,hh,TRUE);h=GetWindow(h,GW_HWNDNEXT);}};
    mv(8,15,48,20); mv(58,y,135,26); mv(200,15,38,20); mv(240,y,145,120); mv(392,y,105,28); mv(502,y,125,28);
    mv(636,15,36,20); mv(674,y,90,140); mv(772,12,105,24); mv(882,y,std::max(160,W-890),140);
    int top=48,left=520,gap=8,settingH=116; MoveWindow(keys_,8,top,left-16,H-top-settingH-gap,TRUE);
    int rightX=left,rightW=std::max(200,W-rightX-8),listH=(H-top-settingH-gap)*2/3; MoveWindow(list_,rightX,top,rightW,listH,TRUE); MoveWindow(log_,rightX,top+listH+gap,rightW,H-top-listH-settingH-2*gap,TRUE);
    int sy=H-settingH+6; MoveWindow(slotLabel_,8,sy,330,22,TRUE); HWND q=GetWindow(slotLabel_,GW_HWNDNEXT); MoveWindow(q,8,sy+30,55,20,TRUE); MoveWindow(volTrack_,68,sy+24,220,32,TRUE); MoveWindow(volEdit_,294,sy+25,55,24,TRUE);
    q=GetWindow(volEdit_,GW_HWNDNEXT); MoveWindow(q,8,sy+66,75,20,TRUE); MoveWindow(trTrack_,86,sy+59,202,32,TRUE); MoveWindow(trEdit_,294,sy+61,55,24,TRUE); MoveWindow(stretch_,365,sy+28,130,24,TRUE);
    MoveWindow(previewLabel_,365,sy+58,130,20,TRUE); MoveWindow(previewTrack_,360,sy+76,140,30,TRUE);
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
    case WM_LBUTTONDOWN:{SetFocus(h);POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};int idx=HitKey(p);if(idx>=0){SelectKey(idx,true);keyHolding_=true;SetCapture(h);InvalidateRect(h,nullptr,FALSE);}return 0;}
    case WM_LBUTTONUP: if(GetCapture()==h)ReleaseCapture(); keyHolding_=false; StopPreview(); InvalidateRect(h,nullptr,FALSE); return 0;
    case WM_RBUTTONDOWN:{
        SetFocus(h); POINT cp{GET_X_LPARAM(l),GET_Y_LPARAM(l)}; int idx=HitKey(cp); if(idx<0)return 0;
        SelectKey(idx,false);
        HMENU menu=CreatePopupMenu(); HMENU wav=CreatePopupMenu();
        AppendMenuW(menu,MF_STRING,1,L"割り当てを削除");
        if(CurSlot().wavPath.empty()) EnableMenuItem(menu,1,MF_BYCOMMAND|MF_GRAYED);
        if(files_.empty()) AppendMenuW(wav,MF_STRING|MF_GRAYED,1000,L"(WAVファイルなし)");
        else for(size_t i=0;i<files_.size();++i) AppendMenuW(wav,MF_STRING,1000+(UINT)i,BaseName(files_[i]).c_str());
        AppendMenuW(menu,MF_POPUP,(UINT_PTR)wav,L"WAVを割り当て");
        POINT sp=cp; ClientToScreen(h,&sp);
        UINT cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON|TPM_NONOTIFY,sp.x,sp.y,0,hwnd_,nullptr);
        if(cmd==1) DeleteFocused();
        else if(cmd>=1000 && cmd<1000+files_.size()) AssignSelected(files_[cmd-1000]);
        DestroyMenu(menu); return 0;
    }
    }
    return DefWindowProc(h,m,w,l);
}

void App::SelectKey(int idx,bool play,float velocity){if(!ValidPdx(idx))return;selected_=idx;int pitch=idx+3;prj_.internalOctave=std::clamp(pitch/12,0,8);UpdateControls();InvalidateRect(keys_,nullptr,FALSE);if(play)PreviewKey(idx,velocity);}
std::vector<float> App::ApplyPreviewGain(const std::vector<float>& in) const{
    float g=std::clamp(prj_.previewVolume,0,500)/100.0f;
    if(std::abs(g-1.0f)<0.0001f)return in;
    std::vector<float> out=in; for(auto& v:out)v=std::clamp(v*g,-1.0f,1.0f); return out;
}
void App::PreviewKey(int idx,float velocity){mousePlayer_.Stop();const auto&s=prj_.banks[prj_.currentBank].slot[idx];if(s.wavPath.empty())return;WaveData w;if(!GetWave(s.wavPath,w))return;auto p=ProcessPcm(w.mono,w.sampleRate,s.volume*velocity,s.transpose,s.pitchShift);auto en=EncodeTarget(p,prj_.format);auto de=DecodeTarget(en.data(),en.size(),prj_.format);auto pv=ApplyPreviewGain(de);mousePlayer_.Play(pv,15625);}
void App::StopPreview(){mousePlayer_.Stop();}

void App::FinishListGesture(){
    if(!listHolding_)return;
    mousePlayer_.Stop();
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
    case WM_LBUTTONDOWN:{
        LVHITTESTINFO hi{};hi.pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};int i=ListView_HitTest(h,&hi);
        if(i>=0&&i<(int)files_.size()){
            // Do not enter the ListView's internal click/drag tracking loop here.
            // Select the row ourselves, start audio immediately on button-down,
            // and keep capture until the physical button is released.
            SetFocus(h);
            ListView_SetItemState(h,-1,0,LVIS_SELECTED|LVIS_FOCUSED);
            ListView_SetItemState(h,i,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
            ListView_SetSelectionMark(h,i); ListView_EnsureVisible(h,i,FALSE);
            dragFile_=i; listHolding_=true;
            mousePlayer_.Stop(); WaveData wd; if(GetWave(files_[i],wd)){auto pv=ApplyPreviewGain(wd.mono);mousePlayer_.Play(pv,wd.sampleRate);}
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

void App::UpdateControls(){if(prj_.banks.empty())prj_.banks.resize(1);prj_.currentBank=std::clamp(prj_.currentBank,0,(int)prj_.banks.size()-1);updating_=true;const auto&s=CurSlot();wchar_t b[512];int pitch=selected_+3;swprintf_s(b,L"Bank %03d / PDX %02d / o%d%s",prj_.currentBank,selected_,pitch/12,L"");std::wstring t=b;if(!s.wavPath.empty())t+=L"  "+BaseName(s.wavPath);SetWindowTextW(slotLabel_,t.c_str());SendMessage(volTrack_,TBM_SETPOS,TRUE,(LPARAM)std::lround(s.volume*100));swprintf_s(b,L"%d",(int)std::lround(s.volume*100));SetWindowTextW(volEdit_,b);SendMessage(trTrack_,TBM_SETPOS,TRUE,s.transpose);swprintf_s(b,L"%+d",s.transpose);SetWindowTextW(trEdit_,b);CheckDlgButton(hwnd_,ID_STRETCH,s.pitchShift?BST_CHECKED:BST_UNCHECKED);SendMessage(bankCombo_,CB_SETCURSEL,prj_.currentBank,0);if(previewTrack_){SendMessage(previewTrack_,TBM_SETPOS,TRUE,prj_.previewVolume);swprintf_s(b,L"試聴音量 %d%%",prj_.previewVolume);SetWindowTextW(previewLabel_,b);}updating_=false;}
void App::ReadControls(){if(updating_||!volEdit_||!trEdit_)return;wchar_t b[64]{};GetWindowTextW(volEdit_,b,64);int v=_wtoi(b);GetWindowTextW(trEdit_,b,64);int t=_wtoi(b);auto&s=CurSlot();s.volume=std::clamp(v,0,200)/100.f;s.transpose=std::clamp(t,-24,24);s.pitchShift=IsDlgButtonChecked(hwnd_,ID_STRETCH)==BST_CHECKED;SendMessage(volTrack_,TBM_SETPOS,TRUE,(LPARAM)std::lround(s.volume*100));SendMessage(trTrack_,TBM_SETPOS,TRUE,s.transpose);s.importedRaw=false;MarkDirty();}

void App::RefreshBankCombo(){if(!bankCombo_)return;updating_=true;SendMessage(bankCombo_,CB_RESETCONTENT,0,0);for(int b=0;b<256;++b){wchar_t s[16];swprintf_s(s,L"%03d",b);SendMessage(bankCombo_,CB_ADDSTRING,0,(LPARAM)s);}SendMessage(bankCombo_,CB_SETCURSEL,prj_.currentBank,0);updating_=false;}
void App::SwitchBank(int bank,bool fromProgramChange){if(bank<0||bank>255)return;if((int)prj_.banks.size()<=bank)prj_.banks.resize((size_t)bank+1);prj_.currentBank=bank;UpdateControls();InvalidateRect(keys_,nullptr,FALSE);MarkDirty();Log((fromProgramChange?L"MIDI Program Change: Bank ":L"Bank切替: ")+std::to_wstring(bank));}

bool App::GetWave(const std::wstring&p,WaveData&o){auto it=cache_.find(p);if(it!=cache_.end()){o=it->second;return true;}std::wstring e;if(!LoadWave(p,o,e)){Log(L"WAV読込エラー: "+BaseName(p)+L" : "+e);return false;}cache_[p]=o;return true;}
void App::AddWavePath(const std::wstring&p){for(auto&x:files_)if(!_wcsicmp(x.c_str(),p.c_str()))return;WaveData w;std::wstring e;if(!LoadWave(p,w,e)){Log(L"無効なWAVを無視: "+BaseName(p)+L" : "+e);return;}files_.push_back(p);cache_[p]=w;AddToList(p,w);Log(L"追加: "+BaseName(p));}
void App::AddFolder(const std::wstring&p){try{for(auto&x:fs::recursive_directory_iterator(p))if(x.is_regular_file()&&!_wcsicmp(x.path().extension().c_str(),L".wav"))AddWavePath(x.path().wstring());}catch(...){Log(L"フォルダ走査エラー: "+p);}}
void App::AddToList(const std::wstring&p,const WaveData&w){int i=ListView_GetItemCount(list_);LVITEMW it{LVIF_TEXT};it.iItem=i;std::wstring bn=BaseName(p);it.pszText=(LPWSTR)bn.c_str();ListView_InsertItem(list_,&it);try{auto ft=fs::last_write_time(p);auto st=std::chrono::time_point_cast<std::chrono::system_clock::duration>(ft-fs::file_time_type::clock::now()+std::chrono::system_clock::now());time_t tt=std::chrono::system_clock::to_time_t(st);tm tmv{};localtime_s(&tmv,&tt);wchar_t b[64];wcsftime(b,64,L"%Y/%m/%d %H:%M",&tmv);ListView_SetItemText(list_,i,1,b);}catch(...){ }wchar_t b[64];swprintf_s(b,L"%u Hz",w.sampleRate);ListView_SetItemText(list_,i,2,b);swprintf_s(b,L"%u bit",w.bits);ListView_SetItemText(list_,i,3,b);std::wstring ch=(w.channels==1?L"モノラル":(w.channels==2?L"ステレオ":std::to_wstring(w.channels)+L" ch"));ListView_SetItemText(list_,i,4,(LPWSTR)ch.c_str());ListView_SetItemText(list_,i,5,(LPWSTR)w.formatName.c_str());}
void App::RebuildFileList(){ListView_DeleteAllItems(list_);for(auto&p:files_){WaveData w;if(GetWave(p,w))AddToList(p,w);}}
void App::SortList(int col){static bool asc[6]={true,true,true,true,true,true};if(col<0||col>=6)return;asc[col]=!asc[col];std::stable_sort(files_.begin(),files_.end(),[&](auto&a,auto&b){WaveData wa,wb;GetWave(a,wa);GetWave(b,wb);int c=0;if(col==0)c=_wcsicmp(BaseName(a).c_str(),BaseName(b).c_str());else if(col==1){auto aa=fs::last_write_time(a),bb=fs::last_write_time(b);c=aa<bb?-1:aa>bb?1:0;}else if(col==2)c=wa.sampleRate<wb.sampleRate?-1:wa.sampleRate>wb.sampleRate?1:0;else if(col==3)c=wa.bits<wb.bits?-1:wa.bits>wb.bits?1:0;else if(col==4)c=wa.channels<wb.channels?-1:wa.channels>wb.channels?1:0;else c=_wcsicmp(wa.formatName.c_str(),wb.formatName.c_str());return asc[col]?c<0:c>0;});RebuildFileList();}
void App::AddDropped(HDROP d){UINT n=DragQueryFileW(d,0xFFFFFFFF,nullptr,0);for(UINT i=0;i<n;i++){wchar_t p[32768];DragQueryFileW(d,i,p,32768);fs::path fp(p);if(fs::is_directory(fp))AddFolder(p);else if(!_wcsicmp(fp.extension().c_str(),L".wav"))AddWavePath(p);else if(!_wcsicmp(fp.extension().c_str(),L".pdx"))ImportPdx(p);else if(!_wcsicmp(fp.extension().c_str(),L".pdxedit"))LoadProjectFile(p,true);else Log(L"無効なドロップを無視: "+BaseName(p));}DragFinish(d);}
void App::Log(const std::wstring&s){if(!log_)return;int len=GetWindowTextLengthW(log_);SendMessage(log_,EM_SETSEL,len,len);std::wstring t=s+L"\r\n";SendMessage(log_,EM_REPLACESEL,FALSE,(LPARAM)t.c_str());SendMessage(log_,EM_SCROLLCARET,0,0);}
void App::MarkDirty(){if(restoringHistory_)return;dirty_=true;changedAt_=TickMs();}
void App::CaptureUiState(){if(!hwnd_)return;WINDOWPLACEMENT wp{sizeof(wp)};if(GetWindowPlacement(hwnd_,&wp)&&wp.showCmd!=SW_SHOWMINIMIZED){RECT r=wp.rcNormalPosition;prj_.windowX=r.left;prj_.windowY=r.top;prj_.windowW=r.right-r.left;prj_.windowH=r.bottom-r.top;}if(list_)for(int i=0;i<6;++i)prj_.listWidths[i]=ListView_GetColumnWidth(list_,i);}
void App::ApplyUiState(){updating_=true;if(prj_.windowW>=640&&prj_.windowH>=480&&prj_.windowX!=CW_USEDEFAULT)SetWindowPos(hwnd_,nullptr,prj_.windowX,prj_.windowY,prj_.windowW,prj_.windowH,SWP_NOZORDER|SWP_NOACTIVATE);for(int i=0;i<6;++i)ListView_SetColumnWidth(list_,i,prj_.listWidths[i]);updating_=false;}
std::wstring App::CurrentProjectPath()const{fs::path dir=fs::path(projectPath_).has_parent_path()?fs::path(projectPath_).parent_path():fs::current_path();std::wstring n=prj_.pdxName.empty()?L"pcm":prj_.pdxName;return(dir/(n+L".pdxedit")).wstring();}
bool App::SaveCurrentNow(){CaptureUiState();std::wstring e,pp=CurrentProjectPath();if(SaveProject(pp,prj_,e)){projectPath_=pp;dirty_=false;std::ofstream lf(fs::current_path()/L"PDXEditor.last",std::ios::binary);lf<<WToUtf8(projectPath_);Log(L"設定保存: "+BaseName(pp));return true;}Log(L"設定保存エラー: "+e);return false;}
void App::SaveAuto(){if(dirty_&&TickMs()-changedAt_>=2000)SaveCurrentNow();}

void App::LoadProjectFile(const std::wstring&p,bool ask){if(ask){std::wstring q=L"「"+BaseName(p)+L"」から編集しますか？";if(MessageBoxW(hwnd_,q.c_str(),L"設定読み込み",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;}ProjectSetting s;std::wstring e;if(!LoadProject(p,s,e)){MessageBoxW(hwnd_,e.c_str(),L"エラー",MB_OK|MB_ICONERROR);return;}prj_=std::move(s);projectPath_=p;files_.clear();cache_.clear();
    for(auto&b:prj_.banks)for(auto&sl:b.slot)if(!sl.wavPath.empty()&&fs::exists(sl.wavPath))AddWavePath(sl.wavPath);
    if(!prj_.sourcePdx.empty()&&fs::exists(prj_.sourcePdx)){ImportedPdx px;std::wstring pe;if(ReadPdx(prj_.sourcePdx,px,pe)){for(size_t b=0;b<prj_.banks.size()&&b<px.table.size();++b)for(int i=0;i<96;++i)if(prj_.banks[b].slot[i].importedRaw&&px.table[b][i].length){auto en=px.table[b][i];prj_.banks[b].slot[i].raw.assign(px.bytes.begin()+en.offset,px.bytes.begin()+en.offset+en.length);}}}
    undo_.clear();redo_.clear();updating_=true;SetWindowTextW(name_,prj_.pdxName.c_str());SendMessage(fmt_,CB_SETCURSEL,(int)prj_.format,0);updating_=false;RefreshBankCombo();selected_=0;ApplyUiState();UpdateControls();InvalidateRect(keys_,nullptr,FALSE);std::ofstream lf(fs::current_path()/L"PDXEditor.last",std::ios::binary);lf<<WToUtf8(projectPath_);Log(L"設定を読み込みました: "+BaseName(p));}

void App::ImportPdx(const std::wstring&p){std::wstring q=L"「"+BaseName(p)+L"」から編集しますか？";if(MessageBoxW(hwnd_,q.c_str(),L"PDX読み込み",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;ImportedPdx x;std::wstring e;if(!ReadPdx(p,x,e)){MessageBoxW(hwnd_,e.c_str(),L"PDXエラー",MB_OK|MB_ICONERROR);return;}auto stem=fs::path(p).stem().wstring();wchar_t ex[MAX_PATH];GetModuleFileNameW(nullptr,ex,MAX_PATH);auto appdir=fs::path(ex).parent_path();auto folder=(appdir/stem).wstring();std::vector<std::array<std::wstring,96>> paths;if(!ExtractPdxAllBanks(p,x,folder,paths,e)){MessageBoxW(hwnd_,e.c_str(),L"展開エラー",MB_OK|MB_ICONERROR);return;}
    prj_=ProjectSetting{};prj_.pdxName=stem;prj_.format=x.guessed;prj_.sourceFormat=x.guessed;prj_.sourcePdx=p;prj_.banks.clear();prj_.banks.resize(std::max(1,x.banks));prj_.currentBank=0;files_.clear();cache_.clear();ListView_DeleteAllItems(list_);
    for(int b=0;b<x.banks;++b)for(int i=0;i<96;++i)if(!paths[b][i].empty()){auto&s=prj_.banks[b].slot[i];s.wavPath=paths[b][i];s.importedRaw=true;auto en=x.table[b][i];s.raw.assign(x.bytes.begin()+en.offset,x.bytes.begin()+en.offset+en.length);AddWavePath(paths[b][i]);}
    projectPath_=(appdir/(stem+L".pdxedit")).wstring();updating_=true;SetWindowTextW(name_,prj_.pdxName.c_str());SendMessage(fmt_,CB_SETCURSEL,(int)prj_.format,0);updating_=false;RefreshBankCombo();selected_=0;UpdateControls();InvalidateRect(keys_,nullptr,FALSE);undo_.clear();redo_.clear();dirty_=true;SaveCurrentNow();Log(L"PDXを展開・設定保存: "+BaseName(p)+L" / bank="+std::to_wstring(x.banks));}

void App::Convert(){
    ReadControls();
    std::vector<std::array<std::vector<uint8_t>,96>> out;
    std::vector<int> sourceBanks;
    int used=0;
    for(int bankNo=0;bankNo<(int)prj_.banks.size() && bankNo<256;++bankNo){
        bool any=false;for(const auto& sl:prj_.banks[bankNo].slot)if(!sl.wavPath.empty()){any=true;break;}
        if(!any)continue;
        std::array<std::vector<uint8_t>,96> ob{};
        for(int i=0;i<96;++i){auto&s=prj_.banks[bankNo].slot[i];if(s.wavPath.empty())continue;
            if(s.importedRaw&&prj_.format==prj_.sourceFormat&&s.volume==1.f&&s.transpose==0&&!s.pitchShift&&!s.raw.empty()){ob[i]=s.raw;++used;continue;}
            WaveData wd;if(!GetWave(s.wavPath,wd))continue;auto pcm=ProcessPcm(wd.mono,wd.sampleRate,s.volume,s.transpose,s.pitchShift);ob[i]=EncodeTarget(pcm,prj_.format);++used;
        }
        sourceBanks.push_back(bankNo);out.push_back(std::move(ob));
    }
    if(out.empty()){MessageBoxW(hwnd_,L"登録されているPCMがありません。",L"変換",MB_OK|MB_ICONINFORMATION);return;}
    std::wstring name=prj_.pdxName.empty()?L"pcm":prj_.pdxName;if(fs::path(name).extension().empty())name+=L".pdx";fs::path base=projectPath_.empty()?fs::current_path():fs::path(projectPath_).parent_path();std::wstring path=(base/name).wstring(),e;std::vector<std::wstring>lg;
    for(size_t i=0;i<sourceBanks.size();++i)if(sourceBanks[i]!=(int)i)Log(L"PDX出力 Bank "+std::to_wstring(sourceBanks[i])+L" -> EX-PDX Bank "+std::to_wstring(i)+L" (空Bankを省略)");
    if(WriteExPdxMulti(path,out,e,&lg)){for(auto&x:lg)Log(x);Log(L"PDX出力完了: "+path+L" ("+std::to_wstring(out.size())+L" bank / "+std::to_wstring(used)+L"音色)");SaveCurrentNow();MessageBoxW(hwnd_,(L"出力しました:\n"+path).c_str(),L"完了",MB_OK|MB_ICONINFORMATION);}else{Log(L"変換エラー: "+e);MessageBoxW(hwnd_,e.c_str(),L"変換エラー",MB_OK|MB_ICONERROR);}
}
void App::RefreshMidi(){SendMessage(midiCombo_,CB_RESETCONTENT,0,0);UINT n=midiInGetNumDevs();for(UINT i=0;i<n;++i){MIDIINCAPSW c{};if(midiInGetDevCapsW(i,&c,sizeof(c))==MMSYSERR_NOERROR)SendMessage(midiCombo_,CB_ADDSTRING,0,(LPARAM)c.szPname);}if(n)SendMessage(midiCombo_,CB_SETCURSEL,0,0);}
void App::CloseMidi(){for(auto&p:midiPlayers_)p.Stop();if(midiIn_){midiInStop(midiIn_);midiInReset(midiIn_);midiInClose(midiIn_);midiIn_=nullptr;}midiDown_.clear();midiPressure_.fill(0.f);channelPressure_=1.f;InvalidateRect(keys_,nullptr,FALSE);}
void App::OpenMidi(){CloseMidi();if(IsDlgButtonChecked(hwnd_,ID_MIDITOGGLE)!=BST_CHECKED)return;int d=(int)SendMessage(midiCombo_,CB_GETCURSEL,0,0);if(d<0)return;if(midiInOpen(&midiIn_,d,(DWORD_PTR)MidiCb,0,CALLBACK_FUNCTION)==MMSYSERR_NOERROR){midiInStart(midiIn_);Log(L"MIDI入力を開始しました");}else{midiIn_=nullptr;CheckDlgButton(hwnd_,ID_MIDITOGGLE,BST_UNCHECKED);Log(L"MIDI入力を開けません");}}
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
                auto pc=ProcessPcm(wd.mono,wd.sampleRate,sl.volume*velocity,sl.transpose,sl.pitchShift);
                auto en=EncodeTarget(pc,prj_.format);auto de=DecodeTarget(en.data(),en.size(),prj_.format);auto pv=ApplyPreviewGain(de);
                midiPlayers_[idx].Play(pv,15625);
            }
        }
    }else if(st==0x80||(st==0x90&&d2==0)){
        midiDown_.erase(idx);midiPlayers_[idx].Stop();
        if(!midiDown_.empty()){selected_=*midiDown_.rbegin();prj_.internalOctave=std::clamp((selected_+3)/12,0,8);UpdateControls();}
    }
    InvalidateRect(keys_,nullptr,FALSE);
}

void App::KeyboardNote(int sem,bool down){if(down){if(keyDownSemis_.count(sem))return;keyDownSemis_.insert(sem);int idx=PitchToPdx(prj_.internalOctave,sem);if(!ValidPdx(idx)){InvalidateRect(keys_,nullptr,FALSE);return;}selected_=idx;UpdateControls();const auto&s=CurSlot();if(!s.wavPath.empty()){WaveData wd;if(GetWave(s.wavPath,wd)){auto pc=ProcessPcm(wd.mono,wd.sampleRate,s.volume,s.transpose,s.pitchShift);auto en=EncodeTarget(pc,prj_.format);auto de=DecodeTarget(en.data(),en.size(),prj_.format);auto pv=ApplyPreviewGain(de);keyPlayers_[sem].Play(pv,15625);}}}else{if(!keyDownSemis_.erase(sem))return;keyPlayers_[sem].Stop();}InvalidateRect(keys_,nullptr,FALSE);}
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
    MarkDirty();
    UpdateControls();
    InvalidateRect(keys_,nullptr,FALSE);
    Log(L"PCM割当: Bank "+std::to_wstring(prj_.currentBank)+L" PDX "+
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
        Log(L"PCMリストから削除: "+std::to_wstring(removed.size())+L"件 / 鍵盤割当解除: "+std::to_wstring(cleared)+L"件");
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
    Log(L"鍵盤割当を削除: Bank "+std::to_wstring(prj_.currentBank)+L" PDX "+
        (selected_<10?L"0":L"")+std::to_wstring(selected_));
}

App::HistoryState App::Snapshot() const{return HistoryState{prj_,files_};}
void App::PushUndo(){if(restoringHistory_)return;undo_.push_back(Snapshot());if(undo_.size()>100)undo_.erase(undo_.begin());redo_.clear();}
void App::RestoreState(const HistoryState&s){restoringHistory_=true;prj_=s.prj;files_=s.files;cache_.clear();RebuildFileList();RefreshBankCombo();updating_=true;SetWindowTextW(name_,prj_.pdxName.c_str());SendMessage(fmt_,CB_SETCURSEL,(int)prj_.format,0);updating_=false;selected_=std::clamp(selected_,0,95);UpdateControls();InvalidateRect(keys_,nullptr,FALSE);restoringHistory_=false;MarkDirty();}
void App::Undo(){if(undo_.empty())return;redo_.push_back(Snapshot());auto s=undo_.back();undo_.pop_back();RestoreState(s);dirty_=true;changedAt_=TickMs();Log(L"Undo (Ctrl+Z)");}
void App::Redo(){if(redo_.empty())return;undo_.push_back(Snapshot());auto s=redo_.back();redo_.pop_back();RestoreState(s);dirty_=true;changedAt_=TickMs();Log(L"Redo (Shift+Ctrl+Z)");}
void App::BeginControlEdit(){if(!pendingControlEdit_)pendingControlEdit_=Snapshot();}
void App::EndControlEdit(){if(!pendingControlEdit_)return;undo_.push_back(*pendingControlEdit_);if(undo_.size()>100)undo_.erase(undo_.begin());redo_.clear();pendingControlEdit_.reset();Log(L"鍵盤設定を変更: Bank "+std::to_wstring(prj_.currentBank)+L" PDX "+std::to_wstring(selected_));}
