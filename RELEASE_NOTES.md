# PDXEditor v1.1.1

## 主な機能と変更

- 波形範囲編集、逆再生、Ctrl+A、Enter／スペースでの試聴。
- 共通設定を実行ファイル横のPDXEditor.iniへ保存。
- 固定レイアウト、直接位置指定スライダー、MIDIデバイス再検出。
- 0BSDライセンス、外部言語と内蔵英語フォールバック。
- Releaseビルドを`/MT`へ変更。再ビルドしたRelease版はVC++再頒布可能パッケージの別途インストール不要。
- PDB参照はファイル名のみとし、ビルド先の絶対パスを除去。

## 配布方針

Windows版ZIPは単一の最上位フォルダー`PDXEditor_v1.1.1`に実行ファイル・言語ファイル・文書を格納します。展開したフォルダー内のPDXEditor.exeを起動してください。既存INIやプロジェクトは保持してください。PDBは同梱しません。

配布用EXEにAuthenticode署名がない場合、初回起動時にWindows SmartScreenの警告が表示されることがあります。

このソース配布物にはEXEを含めません。静的リンクとPDB参照の変更は再ビルド後のEXEに適用されます。検証範囲はVALIDATION.mdを参照してください。

ライセンス：0BSD。Copyright (C) 2026 by I.C.KaZe。
