# MXDRV EX-PDX PCM Editor (VS2022)

Windows 10/11 + Visual Studio 2022 / x64 用 Win32 GUI ツールです。外部ライブラリは不要です。

## 今回の鍵盤仕様

- 画面は o0 ～ o8 の9段、各段は C ～ B の1オクターブです。
- PDX内部番号 `00` は `o0d#`。
- `o0c / o0c# / o0d` は範囲外で選択不可。
- `o8c=93 / o8c#=94 / o8d=95`、`o8d#` 以上は範囲外で選択不可。
- 白鍵・黒鍵とも音名ではなくPDX内部番号 `00`～`95` を表示します。
- 未割当の有効鍵盤は暗色、割当済みは通常色、選択中/発音中は黄緑色です。

## 追加・修正機能

- EX-PDXマルチバンクの読み込み・編集・出力。
- Bankプルダウンは常に000～255を選択可能。Bank追加ボタンは廃止。
- 空BankはPDX出力時に省略し、使用Bankだけを昇順でEX-PDXへ格納します。番号が詰め替わる場合はログに対応関係を表示します。
- MIDI Program Change 0～127 で同番号Bankへ切替。
- PDX読み込み直後に `<PDX名>.pdxedit` を即時保存。
- PCキー `AWSEDFTGYHUJ` はキーリピートを無視し、同時押し可能。
- ↑/↓で内部オクターブ o0～o8 を変更。該当段の背景に暗い選択板を表示。
- 鍵盤描画はメモリDCによるダブルバッファでちらつきを抑制。
- 未割当黒鍵は割当済み黒鍵より明るいグレーにして識別しやすくしています。
- PDX展開WAVは16bit/15.625kHzのモノラルWAVで固定。
- PCMリストに「チャンネル」列を追加し、モノラル/ステレオを表示。
- ウィンドウ位置・サイズ、PCMリスト列幅を設定ファイルに保存。
- Ctrl+Z: Undo、Shift+Ctrl+Z: Redo（最大100手）。
- 鍵盤選択中にDelete: そのBank/内部番号の割当を削除。
- PCMリスト選択中にDelete: リストからPCMを削除し、そのPCMを参照する割当も解除。
- 音量/Transposeは編集終了時またはスライダー操作確定時に1履歴として記録。
- PDX由来で無編集の音色はraw PCMを維持して再出力。

## ビルド

`PDXEditor.sln` をVisual Studio 2022で開き、`Release | x64` でリビルドしてください。
ソースはUTF-8 BOM付き、プロジェクトにも `/utf-8` を指定しています。

## fixed9 additions
- PCM list preview starts immediately on mouse-down and stops on mouse-up; dragging to the keyboard still assigns the selected WAV to the currently selected key.
- Right-clicking a valid keyboard key selects it and opens a context menu to clear the assignment or assign any WAV currently registered in the PCM list.
- Added tool-only preview volume control (0-500%, default 100%). It affects WAV-list, mouse-keyboard, PC-keyboard, and MIDI preview only and does not change exported PDX PCM data. The value is saved in the .pdxedit file.


## ピッチシフト
鍵盤ごとの「ピッチシフト」をONにすると、Transposeで音程を変更しても元WAVと同じ再生時間を維持します。OFFでは再生速度変更型のトランスポーズとなり、音程に応じて再生時間も変化します。


## fixed11
Pitch Shift ON の時間長補正を単純OLAからWSOLA方式へ変更し、Transpose後の音程を維持したまま元の再生時間へ合わせるよう修正。

## fixed14 - PDX互換性強化

ネット上のMDX/PDX互換実装を調査して、古いPDXで問題になる以下の形式差を吸収しました。

- 標準PDXの `offset.l + reserved.w + length.w` を明示的にサポート。予約16bitが0でない古いPDXでも低16bitのlengthを使用して読み込みます。
- EX-PDXの32bit lengthは従来どおりサポート。
- ヘッダ直後にPCMが始まらず、ヘッダとPCMデータの間にコメント/パディングがあるPDXを許容。
- LZX042圧縮PDXはライセンス上の混乱を避けるためデコーダを内蔵せず、未対応形式として明示して読み込みを中止します。
- 通常PDX解析に失敗した場合のみLZX042マーカー `7F FF FF 4C` を検出し、「LZX042圧縮PDXは未対応です」と表示します。

添付B8.PDXに加え、標準PDX、予約領域に非0値を持つ旧PDX、コメント/パディング付きPDX、3バンクEX-PDX、パディング付き3バンクEX-PDXでヘッダ判定を確認しています。LZX042圧縮PDXは未対応として検出・拒否します。

- 768バイト未満の不完全な旧PDXヘッダも、最初のPCM offsetからヘッダ項目数を推定して読み込みます。欠けた末尾項目は未割当として扱います。

## ライセンス上の補足 (fixed14)

LZX042デコーダは内蔵していません。通常PDXとして解析できないファイルにLZX042の識別マーカーが見つかった場合は、未対応形式として読み込みを中止します。LZX042の展開処理・ビットストリーム解析・後方参照展開コードはソースに含まれていません。

## v1.1.0 で追加・変更された仕様（fixed15～fixed32）

fixed14（v1.0.0）公開後に追加・変更された現在の仕様です。上記fixed14時点の記述と異なる場合は、この節の内容が現在の仕様です。

### 言語・表示

- 日本語/英語表示に対応。初回起動時はWindowsのUI言語が日本語なら日本語、それ以外は英語を初期値にします。
- 表示文字列は実行ファイル横の `languages` フォルダにあるUTF-8の `.lang` ファイルから読み込みます。
- `languages\ja.lang` と `languages\en.lang` を同梱しています。既存ファイルを基に別言語の `.lang` を追加すると、言語選択へ追加できます。
- 選択言語は設定ファイルへ保存し、次回起動時に復元します。
- ラベル、ボタン、チェックボックスなどは翻訳後の文字列幅に応じて必要な表示幅を確保します。
- 言語設定はMIDI入力デバイス選択の右側に配置しています。

### PDX / Bank / ファイル読み込み

- `PDXEditor.exe` に `.pdxedit` または `.pdx` をドラッグ＆ドロップして起動できます。
- 「設定/PDXを読み込む」から `.pdxedit` と `.pdx` の両方を選択できます。
- 設定ファイルを読み込んだ場合、その設定ファイルのフォルダを作業カレントフォルダとして扱います。
- PDXを読み込んだ場合の作業カレントフォルダはPDX自身のフォルダではなく、`PDXEditor.exe` のあるフォルダです。
- カレントフォルダ以下のWAVは設定ファイルへ相対パスで保存します。カレント外のファイルは絶対パスを使用します。
- fixed14時点では空Bankを前詰めしていましたが、現在は**最後にデータが存在するBankまでは先頭・途中の空Bankもその番号のまま保持**します。最後の使用Bankより後ろにある末尾の空BankだけをPDX出力から省略します。
- Bank選択は000～255です。
- PDX読込時のPCM形式推定は、極端に短いPCMだけで決めず、複数PCMの波形連続性を使ってADPCM/P8/P16を判定します。

### WAVファイルリスト

- 表示列は、パス、ファイル名、タイムスタンプ、ファイルサイズ、長さ、周波数、ビット数、チャンネル、形式です。
- カレントフォルダ以下のパスは相対表示します。
- Shiftを使用した範囲選択、Ctrlを使用した複数選択、Ctrl+Aによる全選択に対応しています。
- 複数選択状態でDeleteを押すと選択中のWAVをまとめて削除し、それらを参照している鍵盤割当も解除します。
- WAVリストでEnterを押している間、現在のカーソル位置のWAVを試聴できます。
- マウス試聴は押した時点で開始し、離すと停止します。鍵盤へドラッグした場合は現在選択中の鍵盤へ割り当てます。
- WAVリスト上には試聴中WAVの波形全体を半透明で固定表示し、現在の再生位置を縦棒で表示します。
- WAVリストとログの間には上下にドラッグ可能なスプリッターがあり、位置を設定へ保存します。

### 鍵盤・キーボード操作

- ←/→で選択中のPDX内部番号を1つずつ移動します。
- ↑/↓で内部オクターブを変更すると、選択中の鍵盤も-12/+12されます。
- ↑/↓/←/→にはOSのキーリピートを適用します。
- PC演奏キー `AWSEDFTGYHUJ` はキーリピートを無視し、複数同時押しに対応します。
- 鍵盤側でEnterを押している間、現在選択中の音色を試聴します。
- 鍵盤を右クリックすると、その鍵盤を選択して、割当削除または登録済みWAVの割当を選べます。
- 鍵盤を左クリックしている間にマウスホイールを回すと、その鍵盤のTranspose値を変更できます。
- MIDI Note On/Off、Velocity、Program ChangeによるBank切替に対応します。アフタータッチは再発音を伴わず動的に音量変更できないため反映しません。

### 試聴・Transpose / Pitch Shift

- ツール内の試聴音量を0～500%で設定できます。初期値は100%です。
- 試聴音量はWAVリスト、鍵盤、PCキーボード、MIDIの試聴だけに適用し、PDXへ出力するPCMデータ自体には影響しません。
- Pitch Shift ONではTranspose後の音程を維持したまま、元の選択波形と同じ再生時間へ補正します。
- Pitch Shift OFFでは再生速度変更型のTransposeとなり、音程に応じて再生時間も変化します。
- 鍵盤側の試聴中は、試聴音量を掛ける前の現在出力波形を鍵盤上へ半透明で重ねて表示します。複数同時発音時はミックス表示します。

### WAV使用範囲編集

- 選択鍵盤の設定欄に、割り当てられたWAVの波形エディタを表示します。
- 開始位置、終了位置、選択範囲のサンプル数を常時表示します。
- 初期状態ではWAV全体を使用範囲とします。
- 波形上の左ドラッグで使用範囲を指定できます。
- 開始/終了端付近をドラッグすると、その端だけを変更できます。
- マウスホイールでカーソル位置を中心に波形を拡大・縮小できます。ドラッグ操作中にもズームできます。
- 波形上部の水平スクロールバーで、拡大時の表示範囲を移動できます。
- 右クリックメニューから、開始位置・終了位置・両端について内側/外側方向の0クロスポイントを検索できます。
- 選択範囲は元WAVのサンプル番号として設定へ保存されます。
- PDX出力と鍵盤試聴は、`WAV範囲切り出し → Transpose/Pitch Shift → 15.625kHz変換 → ADPCM/P16/P8変換` の順で同じ範囲を使用します。
- 選択範囲は元波形を隠さない半透明オーバーレイで表示します。

### Undo / Redo

- Ctrl+ZでUndo、Ctrl+Shift+ZまたはCtrl+YでRedoします。
- 鍵盤割当、削除、音量、Transpose、Pitch Shift、WAV使用範囲などの編集履歴を対象にします。

### 現在の画面構成

- 右ペインにはWAVファイルリストとログだけを表示します。
- WAVリストとログの間のスプリッター位置は設定ファイルへ保存します。
- 鍵盤・鍵盤設定・WAV範囲編集は左側へ配置します。

## コマンドラインからのビルド

Visual Studio 2022のMSBuildを使用して `Release | x64` をビルドする `build_vs2022.bat` を同梱しています。

`vswhere.exe` は使用しません。BATは次の順でMSBuildを探します。

1. `PATH` にある `MSBuild.exe`
2. ユーザーが指定した `VS2022_PATH`
3. Visual Studio Developer Command Promptの `VSINSTALLDIR`
4. `Program Files` / `Program Files (x86)` 以下のVisual Studio 2022 Community / Professional / Enterprise / BuildTools標準パス

標準外の場所へVisual Studioをインストールしている場合は、例えば次のように指定できます。

```bat
set VS2022_PATH=D:\VisualStudio\2022\Community
build_vs2022.bat
```

ビルド成功時の出力先は `x64\Release\PDXEditor.exe` です。
