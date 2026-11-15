# 依存関係と第三者コード

本体ソースは0BSD（LICENSE参照）、著作権者はI.C.KaZeです。以下のIndependentMDX由来ファイルは、その原著者の通知を保持します。

実行時にWindowsの標準API（Win32、Common Controls、WinMM、Shell、COM、GDI）を使用します。ビルドにはWindows SDK、Visual Studio 2022のC++ツールセット、またはCMake対応のWindows C++ビルド環境が必要です。これらは本アーカイブには同梱していません。

独立した外部音声ライブラリ、LZX042デコーダ、外部ランタイムDLLは同梱していません。PDX形式の互換処理は圧縮PDXの展開機能を含みません。ユーザーが読み込むWAV／PDXの権利は本ソフトウェアのライセンスとは別です。


## MSVCランタイム

MSVCのReleaseビルドは`/MT`、Debugは`/MTd`を指定し、C/C++ランタイムを静的リンクします。この設定で再ビルドしたRelease版について、VC++再頒布可能パッケージの別途インストールは不要です。Windows 10/11に標準で含まれるOSのDLLは引き続き使用します。

MicrosoftのツールセットとランタイムはMicrosoftのライセンスに従います。静的リンクによってMicrosoftのランタイムコードが0BSDへ変更されるものではありません。ランタイムソースや再頒布可能パッケージそのものはこのソースアーカイブに同梱しません。Debugビルドは開発用で、リリース配布には使用しません。


## NDSIFホスト実装

`src/NdsifWire.h` はIndependentMDX 0.34.0の `src/ndsif_wire.h` を使用しています。
フレーム・COBS・CRC・ADPCMエンコーダのコードは0BSDです。
`src/NdsifAudio.cpp` は同版のCOM設定・応答処理・能力確認・ストリーム転送手順を参考に、PDXEditorの共通ミキサーとして実装しています。

```text
Zero-Clause BSD

Copyright (c) 2026 IndependentMDX contributors

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

```

### NanoDrive8のプロトコル参照とライセンスの区別

NanoDrive8の原著コードは、別途明記されたものを除き **BSD 3-Clause**、Copyright (c) 2024-2026, Fujixです。
このリポジトリ全体を0BSDとみなすものではありません。本配布のNDSIFホスト実装は公開プロトコル仕様に基づく独立実装と0BSDのIndependentMDXコードを使用し、NanoDrive8のファームウェア・音源実装・MDXコードは同梱していません。
参照先のBSD 3-Clauseライセンスと本体／ホスト実装の0BSDは別のものです。

- NanoDrive8: https://github.com/Fujix1/NanoDrive8
- 公開API仕様: https://github.com/Fujix1/NanoDrive8/blob/main/NDSIF_API.md
- NanoDrive8 LICENSE: https://github.com/Fujix1/NanoDrive8/blob/main/LICENSE
- IndependentMDX: https://github.com/ickaze/IndependentMDX
