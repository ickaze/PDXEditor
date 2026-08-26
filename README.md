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
