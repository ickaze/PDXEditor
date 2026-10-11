# 依存関係と第三者コード

この配布物のソースは0BSD（LICENSE参照）、著作権者はI.C.KaZeです。

実行時にWindowsの標準API（Win32、Common Controls、WinMM、Shell、COM、GDI）を使用します。ビルドにはWindows SDK、Visual Studio 2022のC++ツールセット、またはCMake対応のWindows C++ビルド環境が必要です。これらは本アーカイブには同梱していません。

独立した外部音声ライブラリ、LZX042デコーダ、外部ランタイムDLLは同梱していません。PDX形式の互換処理は圧縮PDXの展開機能を含みません。ユーザーが読み込むWAV／PDXの権利は本ソフトウェアのライセンスとは別です。


## MSVCランタイム

MSVCのReleaseビルドは`/MT`、Debugは`/MTd`を指定し、C/C++ランタイムを静的リンクします。この設定で再ビルドしたRelease版について、VC++再頒布可能パッケージの別途インストールは不要です。Windows 10/11に標準で含まれるOSのDLLは引き続き使用します。

MicrosoftのツールセットとランタイムはMicrosoftのライセンスに従います。静的リンクによってMicrosoftのランタイムコードが0BSDへ変更されるものではありません。ランタイムソースや再頒布可能パッケージそのものはこのソースアーカイブに同梱しません。Debugビルドは開発用で、リリース配布には使用しません。
