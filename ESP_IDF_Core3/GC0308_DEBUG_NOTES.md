# GC0308デバッグノート

## 現在の状況

### ✅ 確認済み
- PSRAMエラー解消済み
- GC0308サポートは esp32-camera ライブラリに含まれている
- `CONFIG_GC0308_SUPPORT=y` で有効化済み
- ピン設定は正しい（参考コードと一致）
- I2Cポート設定: `sccb_i2c_port=1`

### ❌ 問題
```
E (313) camera: Detected camera not supported.
E (313) camera: Camera probe failed with error 0x106(ESP_ERR_NOT_SUPPORTED)
```

## GC0308検出プロセス

### esp32-cameraの検出フロー
1. `esp_camera_init()` が呼ばれる
2. I2Cバスの初期化
3. 各カメラドライバーの `detect()` 関数を順次呼び出し
4. GC0308の場合: `esp32_camera_gc0308_detect()`
   - I2Cアドレス: `0x21` (GC0308_SCCB_ADDR)
   - PIDレジスタ: `0x00`
   - 期待値: `0x9b` (GC0308_PID)

### 可能性のある原因

1. **I2Cバスの競合**
   - M5Stack CoreS3は内部I2CバスとカメラI2Cバスを持つ
   - 参考コード（Arduino）では `M5.In_I2C.release()` を呼んでいる
   - ESP-IDFでは同様の処理が必要かもしれない

2. **カメラの電源/リセット**
   - カメラモジュールの初期化シーケンスが不足している可能性
   - XCLKの設定が不適切

3. **I2Cタイミング問題**
   - I2Cクロック速度が速すぎる/遅すぎる
   - プルアップ抵抗の問題

## 次のステップ

### 1. I2Cスキャンを実行
GC0308が本当にI2Cバス上に存在するか確認

### 2. カメラログレベルを上げる
`esp_log_level_set("*", ESP_LOG_DEBUG);` でより詳細なログを取得

### 3. 参考実装を調査
- gob_GC0308.hpp ライブラリの内容確認
- M5Stack CoreS3固有の初期化手順があるか確認

### 4. 代替案
- OV2640など他のカメラモジュールでテスト
- M5Stack CoreS3の公式例を確認

## 参考コード（Arduino）の重要ポイント

```cpp
// M5.In_I2C.release(); を呼んで内部I2Cを解放
camera_config.sccb_i2c_port = -1;  // 専用I2C使用
camera_config.xclk_freq_hz = 20000000;  // 20MHz
camera_config.fb_location = CAMERA_FB_IN_PSRAM;  // PSRAMを使用

// 初期化後に補完処理
goblib::camera::GC0308::complementDriver();
```

## 検証すべき設定

- [ ] I2Cアドレススキャン実行
- [ ] カメラログレベルをDEBUGに変更
- [ ] gob_GC0308 補完処理の内容確認
- [ ] XCLK周波数の調整（10MHz, 15MHz, 20MHzで試す）
- [ ] I2Cポート設定の変更（-1, 0, 1で試す）
