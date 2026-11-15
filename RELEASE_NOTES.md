# PDXEditor v1.2.0

v1.1.1を基準にNDSIF音声出力を追加しました。

- Windows Audio／NDSIF、COM番号、再検出・接続を選択可能。
- WAVリスト・鍵盤・PCキー・MIDIの試聴をミックスし、MSM6258互換ADPCMを15.625kHzで転送。
- 能力・クロック・容量確認、fault停止、再接続に対応。
- 出力設定をPDXEditor.iniへ保存。既存の逆再生・範囲編集・設定移行・Undo/Redoを維持。
- 日本語／英語と内蔵英語、ライセンス通知を更新。Releaseは静的ランタイムでデバッグ情報を生成しない設定。

## ダウンロード

- `PDXEditor_v1.2.0_Windows_x64.zip`：Windows 10/11 x64用実行ファイル、言語、文書、ビルド検証結果。
- `PDXEditor_v1.2.0_source.zip`：ソース、VS2022/CMake設定、言語、文書、テスト、Releaseワークフロー。
- `SHA256SUMS.txt`：上記ZIPのSHA-256。

ZIPは単一フォルダー `PDXEditor_v1.2.0` を含みます。Windows版は展開後のPDXEditor.exeを起動してください。既存のPDXEditor.iniとプロジェクトは保持してください。配布物にPDBやユーザー設定は含めません。

NDSIFには対応機器とCOMポートが必要です。別アプリが同じCOMポートを使用している間は接続できません。
未署名のためWindows SmartScreenの確認が表示される場合があります。

## 検証範囲

GitHub Actionsはポータブルコア・NDSIF模擬通信テスト、WindowsでのMSVC Release x64ビルド、EXEのx64形式・外部VC++ランタイム依存・デバッグ情報、ZIP整合性を検査し、成功した場合だけ公開します。実行結果と対象コミットはWindows版のBUILD_VALIDATION.txtを参照してください。
Windows GUIの操作・実際の音声／MIDI・NanoDrive8実機動作は未確認です。詳細はVALIDATION.mdを参照してください。

ライセンス：本体0BSD、Copyright (C) 2026 by I.C.KaZe。IndependentMDX由来コードの著作権と参照元のライセンスはTHIRD_PARTY_NOTICES.mdを参照してください。
