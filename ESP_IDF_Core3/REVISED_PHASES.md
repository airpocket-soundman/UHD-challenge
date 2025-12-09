# 改訂版開発フェーズ計画

## 🎯 戦略変更の理由

PSRAMの問題を後回しにして、まず基本機能を段階的に実装・動作確認することで、確実な開発を進めます。

## 📋 新しいフェーズ構成

### **Phase 2A: カメラ統合（PSRAM不使用）** 🔄 進行中
**目標**: DRAMのみでカメラから画像を取得する

**タスク**:
- [x] カメラドライバ統合（GC0308）
- [x] QVGA (320×240) RGB565形式で取得
- [x] 画像処理パイプライン実装
  - RGB565→RGB888変換
  - 中央クロップ (240×240)
  - Bilinearリサイズ (64×64)
  - 正規化 (float32)
- [x] FPS測定機能
- [x] **PSRAMを無効化**
- [x] **CAMERA_FB_IN_DRAMに変更**
- [x] **クリーンビルド実行**
- [ ] **ビルド完了**（進行中: 1220/1414、86%）← 今ここ
- [ ] **フラッシュ・実機動作確認**

**成功基準**:
- カメラから画像取得できる
- FPS: 最低10以上
- メモリリークなし
- 安定動作

**メモリ使用量**:
- SRAM: ~300KB / 512KB
  - カメラフレームバッファ: 320×240×2 = 153,600 bytes
  - 処理バッファ: ~234,240 bytes
  - プログラム: ~100KB

---

### **Phase 2B: LCD表示統合** 📺 次のフェーズ
**目標**: カメラ画像をLCD画面に表示する

**タスク**:
- [ ] M5Stack CoreS3 LCDドライバ統合
- [ ] ST7789初期化
- [ ] カメラ画像をLCDに転送
- [ ] RGB565フォーマットで直接表示
- [ ] フレームバッファのダブルバッファリング（必要に応じて）

**成功基準**:
- LCD画面にリアルタイムでカメラ映像が表示される
- 画像が正しく表示される（色、解像度）
- FPS: 15以上
- ちらつきが少ない

**実装詳細**:
```cpp
// LCDにカメラ画像を表示
void display_camera_image(camera_fb_t* fb) {
    // RGB565形式でそのまま転送
    lcd_draw_bitmap(0, 0, fb->width, fb->height, fb->buf);
}
```

---

### **Phase 2C: バウンディングボックス描画テスト** 🎨
**目標**: ダミー座標でBBを描画してLCDに表示

**タスク**:
- [ ] 矩形描画関数の実装
- [ ] テキスト描画関数の実装（ラベル用）
- [ ] ダミー検出結果の生成
- [ ] BBとラベルを画像上に描画
- [ ] FPS表示機能

**成功基準**:
- カメラ画像上にBBが正しく描画される
- ラベルとスコアが表示される
- FPS表示が正しい
- 描画処理が軽量（FPS低下が少ない）

**ダミー検出結果**:
```cpp
struct Detection {
    int x, y, w, h;       // バウンディングボックス
    int class_id;         // クラスID
    float confidence;     // 信頼度
    const char* label;    // ラベル
};

// テスト用ダミーデータ
Detection dummy_detections[] = {
    {50, 50, 100, 100, 0, 0.95f, "person"},
    {150, 80, 80, 120, 2, 0.87f, "car"}
};
```

---

### **Phase 3A: PSRAM有効化** 🧠
**目標**: PSRAMを正しく初期化して使用可能にする

**タスク**:
- [ ] M5Stack CoreS3の正しいPSRAM設定を調査
- [ ] sdkconfig設定の最適化
  - Octal vs Quad SPI
  - クロック速度
  - ピン設定
- [ ] PSRAM初期化テスト
- [ ] PSRAMメモリアロケーションテスト

**成功基準**:
- PSRAMが正常に初期化される
- 8MB PSRAMが認識される
- PSRAMへの読み書きが正常に動作
- 起動時エラーなし

**デバッグ方法**:
```cpp
ESP_LOGI(TAG, "PSRAM size: %u bytes", esp_psram_get_size());
ESP_LOGI(TAG, "Free PSRAM: %u bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

// PSRAMにメモリ確保テスト
void* psram_test = heap_caps_malloc(1024*1024, MALLOC_CAP_SPIRAM);
if (psram_test) {
    ESP_LOGI(TAG, "PSRAM allocation success!");
    heap_caps_free(psram_test);
}
```

**調査項目**:
1. M5Stack CoreS3のBSP設定を確認
2. 他のM5Stack CoreS3プロジェクトのPSRAM設定を参考にする
3. ESP-IDF v5.5.1のPSRAM設定ドキュメントを確認

---

### **Phase 3B: SDカードからモデルロード** 💾
**目標**: microSDカードをマウントしてモデルファイルを読み込む

**タスク**:
- [ ] microSD SPI初期化
- [ ] FAT32ファイルシステムマウント
- [ ] model.espdlファイル読み込み
- [ ] model.binファイル読み込み（anchors, wh_scale）
- [ ] PSRAMにモデルデータを配置

**成功基準**:
- microSDカードが認識される
- ファイルが正常に読み込める
- モデルサイズ: ~1.68MB
- 定数データ: 128 bytes
- メモリ確保成功

**実装詳細**:
```cpp
// microSDマウント
esp_vfs_fat_sdmmc_mount(...);

// モデルファイル読み込み
FILE* fp = fopen("/sdcard/model.espdl", "rb");
fseek(fp, 0, SEEK_END);
size_t model_size = ftell(fp);
fseek(fp, 0, SEEK_SET);

// PSRAMに確保
void* model_data = heap_caps_malloc(model_size, MALLOC_CAP_SPIRAM);
fread(model_data, 1, model_size, fp);
fclose(fp);
```

---

### **Phase 3C: AI推論統合** 🤖 最終フェーズ
**目標**: ESP-DLを使用してリアルタイム物体検出を実行

**タスク**:
- [ ] ESP-DL Modelクラスの初期化
- [ ] 推論パイプライン統合
  - カメラ画像取得
  - 前処理（64×64 float32）
  - ESP-DL forward()実行
  - 出力テンソル取得 [1, 56, 8, 8]
- [ ] 後処理（デコード）実装
  - グリッド処理 (8×8)
  - アンカーボックス適用
  - 信頼度フィルタリング
  - NMS（重複除去）
- [ ] 検出結果をLCDに描画
- [ ] パフォーマンス最適化

**成功基準**:
- 推論が正常に実行される
- FPS: 10-20
- 検出精度が良好
- レイテンシ < 150ms

**処理フロー**:
```
[カメラ] → [前処理] → [ESP-DL推論] → [後処理] → [BB描画] → [LCD表示]
   ↓         ↓           ↓            ↓           ↓          ↓
 QVGA     64×64      [1,56,8,8]   Detections   Overlay   Display
 RGB565   float32    pred tensor   bbox+label   Image    @15FPS
```

---

## 📊 メモリ配分（全フェーズ完了後）

### SRAM (512KB)
```
プログラム: ~150KB
スタック: ~64KB
ヒープ: ~300KB
  └─ 作業バッファ
```

### PSRAM (8MB)
```
カメラフレームバッファ: 320×240×2 = 153,600 bytes
モデルデータ: 1.68MB
推論バッファ: ~500KB
中間バッファ: ~300KB
空き: ~5.4MB
```

### Flash (16MB)
```
ブートローダー: 21KB
パーティションテーブル: 3KB
アプリケーション: 3MB
Storage (microSD): 12MB
```

---

## 🔄 現在の状況と次のアクション

### 現在地: **Phase 2A - カメラ統合**

**状態**: PSRAMエラーで起動失敗
```
E (173) octal_psram: PSRAM chip is not connected
E cpu_start: Failed to init external RAM!
abort()
```

### 次のアクション（優先順位順）

#### 1. PSRAMを無効化して Phase 2A を完了 ⭐ 最優先
```powershell
# sdkconfig.defaultsを編集
CONFIG_SPIRAM=n

# main.cppを編集
config.fb_location = CAMERA_FB_IN_DRAM;
config.fb_count = 1;

# クリーンビルド
idf.py fullclean
idf.py build
idf.py -p COM6 flash monitor
```

**期待される出力**:
```
I (xxx) MAIN: Phase 2: Camera integration start
I (xxx) MAIN: Camera initialized
I (xxx) MAIN: Captured first frame: 320x240
I (xxx) MAIN: Captured 30 frames in 1.00 s (30.0 FPS)
```

#### 2. Phase 2A 成功後 → Phase 2B に進む
LCD統合を開始

#### 3. Phase 2B 成功後 → Phase 2C に進む
BB描画テストを実装

#### 4. Phase 2C 成功後 → Phase 3A に進む
PSRAM問題に本格的に取り組む

---

## 📝 各フェーズの所要時間見積もり

| フェーズ | 所要時間 | 備考 |
|---------|---------|------|
| Phase 2A | 1-2時間 | PSRAMを無効化するだけ |
| Phase 2B | 3-4時間 | LCD統合 |
| Phase 2C | 2-3時間 | 描画機能実装 |
| Phase 3A | 4-8時間 | PSRAM問題解決（難易度高） |
| Phase 3B | 2-3時間 | SDカード統合 |
| Phase 3C | 4-6時間 | AI推論統合 |
| **合計** | **16-26時間** | |

---

## 🎯 この戦略の利点

### ✅ メリット
1. **段階的検証**: 各フェーズで動作確認できる
2. **早期の成果**: Phase 2Cでカメラ+LCD+BBが動作する（デモ可能）
3. **リスク分散**: PSRAM問題が解決できなくても基本機能は動作
4. **デバッグが容易**: 問題の切り分けが明確
5. **モチベーション維持**: 目に見える進捗

### ⚠️ 注意点
1. PSRAM なしでは処理速度が遅い可能性
2. メモリが不足する可能性（SRAM 512KB のみ）
3. Phase 3A でPSRAM問題が解決できないリスク

### 💡 対策
- Phase 2A-2C の実装を最適化してメモリ使用量を削減
- PSRAM問題は並行して調査を進める
- 最悪の場合、PSRAM なしでも動作するように設計

---

## 🚀 今すぐやること

### **Phase 2A を完了させましょう！**

1. **sdkconfig.defaultsを編集**
   ```ini
   CONFIG_SPIRAM=n
   ```

2. **main.cppを編集**
   ```cpp
   config.fb_location = CAMERA_FB_IN_DRAM;
   ```

3. **ビルド・フラッシュ**
   ```powershell
   idf.py fullclean
   idf.py build
   idf.py -p COM6 flash monitor
   ```

4. **動作確認**
   - カメラ初期化成功
   - フレーム取得成功
   - FPS表示

---

**準備はできましたか？Phase 2A を完了させましょう！** 🚀
