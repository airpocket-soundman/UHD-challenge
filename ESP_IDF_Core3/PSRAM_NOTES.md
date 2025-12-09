# PSRAM有効化のための調査メモ

## 📋 M5Stack CoreS3 公式実装の調査結果

### 参考コード
M5StackCoreS3_CameraWebServerの実装より

```cpp
// M5Stack CoreS3公式のカメラ設定
static camera_config_t camera_config = {
    .pin_pwdn     = -1,
    .pin_reset    = -1,
    .pin_xclk     = 2,
    .pin_sscb_sda = 12,
    .pin_sscb_scl = 11,
    .pin_d7 = 47,
    .pin_d6 = 48,
    .pin_d5 = 16,
    .pin_d4 = 15,
    .pin_d3 = 42,
    .pin_d2 = 41,
    .pin_d1 = 40,
    .pin_d0 = 39,
    .pin_vsync = 46,
    .pin_href  = 38,
    .pin_pclk  = 45,
    .xclk_freq_hz = 20000000,
    .ledc_timer   = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_RGB565,
    .frame_size   = FRAMESIZE_QVGA,        // 320×240
    .jpeg_quality = 0,
    .fb_count     = 2,                     // ダブルバッファ
    .fb_location  = CAMERA_FB_IN_PSRAM,    // ← PSRAMを使用！
    .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
    .sccb_i2c_port = -1,
};
```

### 重要な発見

#### 1. PSRAMは公式実装で使用されている
M5Stack CoreS3の公式コードでは**PSRAMを使用**しています。
- フレームバッファをPSRAMに配置: `CAMERA_FB_IN_PSRAM`
- これは動作することが証明されている

#### 2. GC0308カメラには補完ドライバが必要
```cpp
if(!goblib::camera::GC0308::complementDriver())
{
    M5_LOGE("Failed to complement GC0308");
}
```
- GC0308カメラには特別な補完ドライバが必要
- これがないとカメラが正常に動作しない可能性

#### 3. M5Unified vs ESP-IDF
公式実装は`M5Unified`ライブラリを使用：
```cpp
M5.begin();
M5.In_I2C.release();  // I2Cバスをリリース
camera_config.sccb_i2c_port = M5.In_I2C.getPort();
```

私たちはESP-IDFのみで実装しているため、この部分は不要。

---

## 🔍 PSRAM問題の原因分析

### 現在のエラー
```
E (173) octal_psram: PSRAM chip is not connected, or wrong PSRAM line mode
E cpu_start: Failed to init external RAM!
abort()
```

### 考えられる原因

#### 1. PSRAMの接続モード設定が間違っている
**現在の設定（sdkconfig.defaults）**:
```ini
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=n      # Octal SPI無効
CONFIG_SPIRAM_MODE_QUAD=y     # Quad SPI有効
CONFIG_SPIRAM_SPEED_80M=y     # 80MHz
```

**可能性**:
- M5Stack CoreS3のPSRAMはOctal SPIかもしれない
- クロック速度が間違っているかもしれない
- ピン設定が必要かもしれない

#### 2. ESP-IDF v5.5.1との互換性
- ESP-IDF v5.5.1でのPSRAM設定方法が変わった可能性
- M5Stack CoreS3のBSP（Board Support Package）が必要かもしれない

#### 3. 初期化順序
- M5Unifiedライブラリは特別な初期化手順を実行している可能性
- GPIO初期化やクロック設定などの前処理が必要

---

## 🎯 Phase 3A: PSRAM有効化の戦略

### 戦略1: M5Stack CoreS3 BSPを使用
**メリット**:
- 公式サポート
- 設定が簡単
- 動作保証

**デメリット**:
- 追加の依存関係
- ESP-IDFのみの実装ではなくなる

**実装方法**:
```yaml
# idf_component.yml に追加
dependencies:
  m5stack/m5cores3: "^1.0.0"  # M5Stack CoreS3 BSP
```

### 戦略2: sdkconfig設定を試行錯誤
**試すべき設定パターン**:

#### パターンA: Octal SPI
```ini
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_MODE_QUAD=n
CONFIG_SPIRAM_SPEED_80M=y
```

#### パターンB: Quad SPI + 異なるクロック
```ini
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_QUAD=y
CONFIG_SPIRAM_SPEED_40M=y
```

#### パターンC: 自動検出
```ini
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_AUTO=y
```

### 戦略3: M5Stack公式の設定を調査
M5Stack CoreS3の公式プロジェクトから`sdkconfig`をコピーする

---

## 📝 Phase 3A実装計画

### Step 1: 現在の戦略（Phase 2A完了後）
1. **PSRAMを一時的に無効化してカメラを動作させる** ← 今ここ
   - `CONFIG_SPIRAM=n`
   - `CAMERA_FB_IN_DRAM`
2. Phase 2A完了を確認

### Step 2: PSRAM設定の調査（Phase 3A）
1. M5Stack CoreS3の公式プロジェクトを調査
   - GitHub: m5stack/M5CoreS3
   - sdkconfig設定を確認
   - PSRAM関連の設定をすべて抽出

2. ESP-IDF v5.5.1のPSRAMドキュメントを確認
   - Octal SPI PSRAM設定
   - ESP32-S3のPSRAM設定例

3. 他のESP32-S3プロジェクトを参考にする
   - PSRAMを使用しているプロジェクトを検索
   - 設定を比較

### Step 3: PSRAM有効化の試行
1. **試行1: Octal SPI設定**
   ```ini
   CONFIG_SPIRAM=y
   CONFIG_SPIRAM_MODE_OCT=y
   CONFIG_SPIRAM_SPEED_80M=y
   ```

2. **試行2: Quad SPI設定**
   ```ini
   CONFIG_SPIRAM=y
   CONFIG_SPIRAM_MODE_QUAD=y
   CONFIG_SPIRAM_SPEED_80M=y
   ```

3. **試行3: M5Stack BSP使用**
   - BSPを追加
   - BSPの設定を使用

### Step 4: 動作確認
```cpp
extern "C" void app_main(void)
{
    // PSRAM初期化確認
    size_t psram_size = esp_psram_get_size();
    ESP_LOGI(TAG, "PSRAM size: %u bytes", psram_size);
    
    if (psram_size > 0) {
        ESP_LOGI(TAG, "PSRAM initialized successfully!");
        
        // PSRAMにメモリ確保テスト
        void* test_mem = heap_caps_malloc(1024*1024, MALLOC_CAP_SPIRAM);
        if (test_mem) {
            ESP_LOGI(TAG, "PSRAM allocation test: SUCCESS");
            heap_caps_free(test_mem);
        } else {
            ESP_LOGE(TAG, "PSRAM allocation test: FAILED");
        }
    } else {
        ESP_LOGE(TAG, "PSRAM not available");
    }
}
```

---

## 🔗 参考リンク

### M5Stack関連
- [M5Stack CoreS3 公式ドキュメント](https://docs.m5stack.com/en/core/CoreS3)
- [M5Stack CoreS3 GitHub](https://github.com/m5stack/M5CoreS3)
- [M5Stack Arduino Examples](https://github.com/m5stack/M5Stack/tree/master/examples)

### ESP-IDF関連
- [ESP-IDF PSRAM Configuration](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-guides/external-ram.html)
- [ESP32-S3 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
  - Chapter: PSRAM Controller

### GC0308カメラ
- [GC0308 Datasheet](https://github.com/m5stack/M5-Camera-Tool/blob/master/datasheet/GC0308.pdf)
- goblib GC0308補完ドライバ実装を調査

---

## 💡 現時点での推奨アプローチ

### Phase 2A: まずカメラを動作させる（PSRAM不使用）
✅ **今すぐ実行**:
1. PSRAMなし（DRAM）でカメラ動作確認
2. 画像取得とFPS測定
3. Phase 2B（LCD統合）に進む
4. Phase 2C（BB描画）まで完成させる

### Phase 3A: その後PSRAM問題に集中
🎯 **Phase 2Cが完了してから**:
1. M5Stack CoreS3の公式sdkconfig設定を調査
2. PSRAM設定を試行錯誤
3. 必要に応じてBSPを導入
4. PSRAM有効化が完了したら、カメラをPSRAMに移行

---

## 🚀 期待される結果

### PSRAM有効化成功時
```
I (xxx) boot: ESP-IDF v5.5.1 2nd stage bootloader
I (xxx) boot: Multicore bootloader
I (xxx) boot: chip revision: v0.2
I (xxx) psram: Found 8MB PSRAM device
I (xxx) psram: PSRAM initialized, cache is in Normal mode
I (xxx) MAIN: PSRAM size: 8388608 bytes
I (xxx) MAIN: PSRAM allocation test: SUCCESS
```

### カメラがPSRAMのフレームバッファで動作
```
I (xxx) MAIN: Camera initialized
I (xxx) MAIN: Frame buffer location: PSRAM
I (xxx) MAIN: Captured first frame: 320x240
I (xxx) MAIN: Captured 30 frames in 1.00 s (30.0 FPS)
```

---

**現在の優先度**: Phase 2A完了 > Phase 2B > Phase 2C > Phase 3A

PSRAMはPhase 3Aで解決します。今は基本機能の実装を優先！
