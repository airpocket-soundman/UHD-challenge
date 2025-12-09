# Phase 2A フラッシュ・テスト手順

## 🎯 現在の状況
- ✅ PSRAMを無効化（CONFIG_SPIRAM=n）
- ✅ CAMERA_FB_IN_DRAMに変更
- ✅ クリーンビルド実行
- 🔄 ビルド完了間近（1375/1414、97%）

---

## 📝 ビルド完了後の手順

### ステップ1: ビルドサイズを確認
ビルドが完了したら、バイナリサイズを確認：
```
Project build complete. To flash, run:
 idf.py -p (PORT) flash
or
 idf.py -p (PORT) flash monitor
```

### ステップ2: デバイスをフラッシュ
デバイスをPCに接続してフラッシュ：
```powershell
cd c:\Users\yamas\Documents\GitHub\UHD-challenge\ESP_IDF_Core3
& 'C:\Espressif\frameworks\esp-idf-v5.5.1\export.ps1'
idf.py -p COM6 flash monitor
```

**重要**: COM6が正しいポートか確認してください

### ステップ3: モニター出力を観察

#### 期待される出力（成功時）
```
I (xxx) boot: ESP-IDF v5.5.1 2nd stage bootloader
I (xxx) boot: Multicore bootloader
I (xxx) boot: chip revision: v0.2
I (xxx) boot: Compile time: Dec 9 2025 09:20:00
I (xxx) boot: Multicore bootloader
I (xxx) cpu_start: Pro cpu start user code
I (xxx) cpu_start: cpu freq: 240000000 Hz
I (xxx) app_init: Application information:
I (xxx) app_init: Project name: uhd_detection_core3
I (xxx) app_init: App version: ea5a5f3-dirty
I (xxx) app_init: Compile time: Dec 9 2025 09:23:44
I (xxx) app_init: ELF file SHA256: xxxxxxxxxxxx
I (xxx) MAIN: Phase 2A: Camera integration (DRAM only)
I (xxx) MAIN: [boot] Free heap: XXXXXX bytes
I (xxx) MAIN: Initializing camera (GC0308, QQVGA, RGB565)...
I (xxx) MAIN: Pins: XCLK=2, SDA=12, SCL=11, VSYNC=46, HREF=38, PCLK=45
I (xxx) MAIN: Sensor detected: ID=0x9b (expect GC0308)
I (xxx) MAIN: Sensor configured (QVGA, RGB565)
I (xxx) MAIN: Camera initialized
I (xxx) MAIN: Buffers allocated
I (xxx) MAIN: [after buffers] Free heap: XXXXXX bytes
I (xxx) MAIN: Captured first frame: 320x240 format=2 len=153600
I (xxx) MAIN: Captured 30 frames in 1.00 s (30.0 FPS). Free heap: XXXXXX bytes
```

#### 確認ポイント
- ✅ **PSRAMエラーが出ないこと**
  - ❌ 以前のエラー: `E (173) octal_psram: PSRAM chip is not connected`
  - ✅ 今回は出ないはず
  
- ✅ **カメラ初期化が成功**
  - `I (xxx) MAIN: Camera initialized`
  
- ✅ **フレーム取得が成功**
  - `I (xxx) MAIN: Captured first frame: 320x240`
  
- ✅ **FPSが表示される**
  - 目標: 10 FPS以上
  - 理想: 30 FPS
  
- ✅ **メモリリークがない**
  - Free heapが安定している

### ステップ4: トラブルシューティング

#### 問題A: PSRAMエラーがまだ出る
```
E (173) octal_psram: PSRAM chip is not connected
E cpu_start: Failed to init external RAM!
```
**原因**: sdkconfigがまだPSRAM有効になっている
**対処**:
```powershell
idf.py reconfigure
idf.py build
idf.py -p COM6 flash monitor
```

#### 問題B: カメラ初期化失敗
```
E (xxx) MAIN: Camera init failed: ESP_ERR_NOT_FOUND
```
**原因**: I2C通信エラー、カメラセンサーが認識されない
**対処**:
1. ピン接続を確認
2. カメラケーブルを再接続
3. デバイスを再起動

#### 問題C: メモリ不足
```
E (xxx) MAIN: Failed to allocate image buffers
```
**原因**: DRAM不足（512KBのみ）
**対処**:
1. フレームバッファサイズを削減
2. 処理バッファを最適化
3. 不要なコンポーネントを無効化

#### 問題D: フレーム取得失敗
```
E (xxx) MAIN: Failed to acquire camera frame
```
**原因**: カメラ設定エラー
**対処**:
1. フレームサイズを確認（QVGA）
2. クロック周波数を調整（20MHz）
3. grab_modeを確認（CAMERA_GRAB_WHEN_EMPTY）

### ステップ5: 成功基準の判定

#### ✅ Phase 2A完了条件
以下をすべて満たす必要があります：

1. ✅ ビルド成功
2. ❓ 起動成功（PSRAMエラーなし）
3. ❓ カメラ初期化成功
4. ❓ 画像取得成功（最低1フレーム）
5. ❓ FPS: 10以上
6. ❓ メモリリークなし（1分間安定動作）

#### ⚠️ 許容される問題
- FPSが30未満（10以上なら可）
- 時々フレームドロップ
- 起動時のワーニングメッセージ

#### ❌ 許容できない問題
- PSRAMエラーが出る
- カメラ初期化失敗
- クラッシュする
- 画像が取得できない

---

## 📊 テスト結果記録フォーマット

ビルド完了後、以下の情報を記録してください：

### ビルド情報
```
ビルド日時: 2025年12月9日 午前9時23分
バイナリサイズ: _____ KB
ビルド時間: _____ 秒
ESP-IDF バージョン: v5.5.1
```

### 起動ログ
```
[ここに起動ログをコピー]
```

### FPS測定結果
```
最小FPS: _____
最大FPS: _____
平均FPS: _____
```

### メモリ使用量
```
起動時Free heap: _____ bytes
バッファ確保後Free heap: _____ bytes
動作中Free heap (最小): _____ bytes
動作中Free heap (最大): _____ bytes
```

### 判定
- [ ] **Phase 2A完了**: すべての条件を満たした
- [ ] **Phase 2A部分成功**: 一部の条件のみ満たした
- [ ] **Phase 2A失敗**: 重大な問題が発生

### 備考
```
[問題点、気づいた点、改善点など]
```

---

## 🎉 Phase 2A完了後

### 次のステップ: Phase 2B（LCD統合）
Phase 2Aが成功したら、次はLCD表示統合に進みます：

1. M5Stack CoreS3のLCDドライバを統合
2. カメラ画像をLCDに表示
3. リアルタイム映像表示を実現

**所要時間**: 3-4時間

---

## 🛠️ モニター操作

### モニター終了
```
Ctrl + ]
```

### ログ保存（オプション）
```powershell
idf.py -p COM6 monitor > phase2a_test_log.txt
```

### 再フラッシュ（必要に応じて）
```powershell
idf.py -p COM6 flash
```

---

## 📞 サポート

問題が発生した場合：
1. エラーメッセージを記録
2. PHASE2A_CHECKLIST.mdを参照
3. トラブルシューティングセクションを確認

---

**現在の状態**: ビルド進行中（97%完了）

**次のアクション**: ビルド完了を待ち、フラッシュ・モニターを実行
