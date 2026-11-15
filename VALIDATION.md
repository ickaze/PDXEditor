# PDXEditor v1.2.0 検証記録

検証日：2026-11-15。基準：GitHub mainのv1.1.1（52ada92446f695639ea02db77615b10c633965bd）へNDSIF出力を移植したソース。

## 今版で実施

- `python3 tests/run_portable_checks.py`：実ソースの設定・言語・WAV・PDX処理をGCC/C++17でコンパイルして検証。
- 既存の逆再生・範囲設定の保存、WAV／ADPCM／P16／P8、単一／複数バンクPDXの読み書きを確認。
- 新しい音声出力設定のINI保存・読込、共通設定コピー、INI優先、旧NDSIFプロジェクトからの未設定項目移行を確認。
- 内蔵英語と外部日本語／英語の追加表示キー、ビルドソース登録、Release静的ランタイム・デバッグ情報無効化、BATのCRLFを確認。
- 実際の `src/NdsifAudio.cpp` を模擬Win32 APIでコンパイルし、CRC既知値、0～256byteのCOBS往復と破損拒否、分割応答読み取りを確認。
- 非対応ストリーム・固定4MHz・能力ページ循環の拒否、max_payload=64／data_capacity=256での容量制御、同時発音と異なる入力レートの再生位置・終了・離鍵、fault停止と再接続を確認。

模擬通信テスト（GCC/C++17）：

```sh
g++ -std=c++17 -Wall -Wextra -Werror -pthread -Itests/mock -Isrc tests/ndsif_audio_test.cpp src/NdsifAudio.cpp -o ndsif_audio_test
./ndsif_audio_test
```

`tests/mock/windows.h` は模擬通信テスト専用で、通常のWindowsビルドには使用しません。

## 今版の未確認範囲

ローカル環境ではWindows／VS2022によるGUI全体のビルドは未実施です。GitHub Actionsでのビルド・PE検査結果はWindows配布ZIPのBUILD_VALIDATION.txtに記録します。描画・入力・MIDI・waveOut、NanoDrive8実機出力、通信遅延・音質・長時間稼働は未確認です。模擬APIとASCIIパスを使う検証は、Windowsのドライバーや日本語パス・権限を検証しません。
ソースアーカイブはソースのみです。別途Windows配布ZIPをGitHub Actionsで生成する際、インポートテーブルとデバッグディレクトリーを検査し、結果をBUILD_VALIDATION.txtへ記録します。GUIの実行確認は行いません。

## 同梱物

ソース、VS/CMake設定、CRLFのBAT、日本語／英語、LICENSE、第三者通知、README、CHANGELOG、RELEASE_NOTES、検証コードを含みます。EXE・DLL・PDB・ユーザーINI・WAV・PDXは含みません。
SHA256SUMS.txtは同梱ファイルのハッシュ一覧で、自身を除外します。

---

## 以下はv1.1.1の過去の検証記録

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
