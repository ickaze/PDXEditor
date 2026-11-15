# PDXEditor v1.2.0

v1.1.1を基準にNDSIF音声出力を追加したソース更新です。

- Windows Audio／NDSIF、COM番号、再検出・接続を選択可能。
- WAVリスト・鍵盤・PCキー・MIDIの試聴をミックスし、MSM6258互換ADPCMを15.625kHzで転送。
- 能力・クロック・容量確認、fault停止、再接続に対応。
- 出力設定をPDXEditor.iniへ保存。既存の逆再生・範囲編集・設定移行・Undo/Redoを維持。
- 日本語／英語と内蔵英語、ライセンス通知を更新。Releaseは静的ランタイムでデバッグ情報を生成しない設定。

## 配布内容

`PDXEditor_v1.2.0_source.zip` は単一フォルダー `PDXEditor_v1.2.0` にソース・ビルド設定・言語・文書・テストを収録します。EXE・DLL・PDB・INI・ユーザープロジェクトは含みません。
`build_vs2022.bat` または `Release | x64` で再ビルドしてください。既存のINIとプロジェクトは保持してください。

## 検証範囲

ポータブルコアの設定／WAV／PDX回帰検証とNDSIF模擬通信検証を実行しました。Windows上の全体ビルド・GUI・音声・MIDI・NanoDrive8実機動作は未確認です。詳細はVALIDATION.mdを参照してください。

ライセンス：本体0BSD、Copyright (C) 2026 by I.C.KaZe。IndependentMDX由来コードの著作権と参照元のライセンスはTHIRD_PARTY_NOTICES.mdを参照してください。
