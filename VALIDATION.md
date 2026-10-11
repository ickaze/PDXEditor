# PDXEditor v1.1.1 検証記録

検証日：2026-10-11。対象：このソース配布物。

## 実施済み

Linux環境のPython 3とg++（C++17）で`tests/run_portable_checks.py`を実行し、成功しました。

このスクリプトは配布物のAudio.cpp・Pdx.cpp・Settings.cpp・Localization.hを読み込み、Windows APIの一部にテスト用の代替を与えてコンパイルします。音声デバイス処理はコンパイル対象から除外します。テスト用の文字コード変換はASCII入力に限定され、WindowsのUTF-8／日本語パス処理を検証するものではありません。

- 言語ファイルがない場合に、ウインドウタイトルのIDではなく内蔵英語を取得。
- ソースで参照する文字列IDが日本語／英語の言語ファイルに存在。
- プロジェクトの保存・読み込みで範囲と逆再生設定を保持。
- INIの保存・読み込みでウインドウ設定を保持。
- INIにある幅を優先し、INIにない高さを旧プロジェクトから取得。
- 合成PCMから16bitモノラルWAVを作成して読み込み、サンプル数を確認。
- ADPCM／P16／P8の符号化・復号化後のサンプル数を確認（音質評価ではありません）。
- 合成PCMから単一バンクPDXと3バンクEX-PDXを作成して解析し、途中の空バンクと格納データの長さを確認。
- Visual StudioプロジェクトXMLを解析し、ビルド後コピーの設定を確認。
- CMakeのビルド後コピー設定と、WSOLAの関数名を確認。

合成データはテスト実行時の一時フォルダーで生成・削除します。B8.PDXなどの第三者ファイルは配布物に含めず、今回の検証には使用していません。

再実行：

```sh
python3 tests/run_portable_checks.py
```

## 未実施

- Visual Studio 2022／Windows SDKによる実ビルド。
- Windows上でのビルド後コピー、EXEの起動、日本語／英語GUI、レイアウト、描画、キー入力。
- MIDIデバイスの接続・切断、waveOutによる実音声再生。
- 実在するPDX／EX-PDXコレクションを用いた互換性・聴感確認。
- 日本語や非ASCIIのファイルパス、Windows固有のファイル権限・保存動作。

ソース内の互換処理の存在は、上記の未実施項目の成功を保証しません。Windowsでのビルド方法はREADMEに記載しています。

## 配布物の確認

LICENSE、README、CHANGELOG、依存関係文書、日本語／英語言語ファイル、ビルド設定、テストスクリプトを同梱します。EXE、INI、ユーザー作成プロジェクト、WAV／PDXは同梱しません。SHA256SUMS.txtは同梱ファイルのSHA-256一覧です（一覧ファイル自身は除外）。


## 2026-10-11：ランタイム／PDB設定のソース更新

Visual StudioプロジェクトのRelease `MultiThreaded`／Debug `MultiThreadedDebug`と、CMakeのCMP0091／MSVC_RUNTIME_LIBRARY設定を照合しました。両方のリンカー指定に`/PDBALTPATH:%_PDB%`があることを確認しました。

今回の更新にはEXEを含めません。Windowsでの再ビルド、EXEのインポートテーブルからの動的VC++ランタイム依存の除去、CodeView PDB参照がファイル名のみになることは未確認です。新しいEXEを配布する際に確認が必要です。PDBそのものにはソースパス等が含まれ得るため公開ZIPへ含めません。
