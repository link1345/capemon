# APIコール計測の単体回帰テスト

CAPE、ゲスト、モニターDLLの注入は不要。Visual Studioの **x86 / x64 Native Tools Command Prompt** で、それぞれリポジトリのルートから実行する。

```bat
python tests\prepare-api-call-metrics-test.py
cl /nologo /O2 /W3 /DMONGO_HAVE_STDINT /DMONGO_STATIC_BUILD /Iobjects\metrics-tests /Idistorm\include tests\api-call-metrics.c bson\bson.c bson\encoding.c bson\numbers.c /Feobjects\metrics-tests\metrics-test.exe /Foobjects\metrics-tests\ /link psapi.lib
objects\metrics-tests\metrics-test.exe
```

`NDEBUG` を指定しないこと。assertで結果を検証する。生成物はgitignore対象の `objects/metrics-tests/` に保存される。

テストは現在の `hooking_32.c` / `hooking_64.c` からトランポリン生成関数を抽出し、テストプロセス内で作った関数を呼び出す。外部プロセスやシステムDLLは変更しない。`log.c` のBSON計測出力関数も抽出し、実際のBSONライブラリで検証する。

検証対象:

- 内側・外側の開始値、同じAPIの再帰、連続呼び出しごとの記録。
- DLL通知相当の補助ログが計測状態を変更しないこと。
- NOTAILのNew_ / Alt_両経路、4・5・6引数のスタック位置。
- 未記録・巻き戻された内側フレームの破棄、同じスタック位置の再利用、64フレーム超過時の欠損と外側の保持。
- BSONのQPC値と時間計算、ゼロ時間、開始・終了取得失敗、逆転QPC、不正周波数による時間項目の欠損。メモリ項目は残る。

このテストでは本番のフック受理判定、実際のDLL通知配信、スレッド切替、CAPEv2の保存・API・Web経路は検証しない。

## CAPE導入済み環境での確認

新しいWin32/x64モニターDLLを配置し、whoamiと報告のAgentarium解析を再実行する。定期計測の無効・有効をそれぞれ確認する。

1. ネストしたAPIの開始値が独立し、`qpc_end >= qpc_start`、`qpc_frequency > 0`、`duration_us = (qpc_end - qpc_start) * 1000000 / qpc_frequency` を満たす。
2. `DllLoadNotification` は時間項目が欠損し、外側の `LoadLibraryExW` / `LdrLoadDll` の時間項目を消費しない。通知ログのメモリ項目は残る。
3. BSON、保存JSON、APIレスポンス、Webで値と欠損状態が一致する。現在のCAPEv2に計測項目の取り込み・表示対応があることも確認する。
4. 同一APIの連続呼び出しを数え、モニターのBSONで各 `r=0`、下流でも `repeated` に集約されていないことを確認する。
5. `api-call-metrics=0` では時間・メモリの計測項目が出力されない。
