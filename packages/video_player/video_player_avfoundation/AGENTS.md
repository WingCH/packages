# video_player_avfoundation 開發指南

使用 AVFoundation 實作的 `video_player` iOS 與 macOS 版本。採用聯合插件架構，共享 Darwin 原始碼。

## 快速參考

```bash
# 所有命令在套件根目錄執行：packages/video_player/video_player_avfoundation

# Dart 測試
fvm flutter test                                       # 執行所有測試
fvm flutter test test/avfoundation_video_player_test.dart  # 執行單一檔案
fvm flutter test --name "create with asset"            # 依名稱執行單一測試

# 分析與格式化
fvm flutter analyze                                    # 靜態分析（不要用 flutter build）
dart format .                                          # 格式化 Dart 程式碼

# 程式碼生成
dart run build_runner build -d                         # 重新生成 mock
dart run pigeon --input pigeons/messages.dart          # 重新生成 Pigeon 程式碼
```

## 建置與測試命令

### Dart 單元測試
```bash
fvm flutter test test/avfoundation_video_player_test.dart
```

### 原生測試（XCTest）
測試檔案位於 `darwin/RunnerTests/VideoPlayerTests.m`，透過 Xcode 執行：
```bash
# iOS
cd example && fvm flutter build ios --config-only && open ios/Runner.xcworkspace
# macOS  
cd example && fvm flutter build macos --config-only && open macos/Runner.xcworkspace
```
然後在 Xcode 按 Cmd+U 執行測試。

### 整合測試
```bash
cd example && fvm flutter test integration_test/video_player_test.dart
```

## 程式碼生成

### Pigeon（平台通道）
**修改 `pigeons/messages.dart` 後必須重新生成：**
```bash
dart run pigeon --input pigeons/messages.dart
```
生成檔案：
- `lib/src/messages.g.dart`（Dart）
- `darwin/.../messages.g.h` + `messages.g.m`（Objective-C）

### Mockito（測試 Mock）
**修改被 mock 的介面後需重新生成：**
```bash
dart run build_runner build -d
```
生成檔案：`test/avfoundation_video_player_test.mocks.dart`

## 程式碼風格

### Dart
- **風格**：Flutter 風格指南
- **格式化工具**：`dart format`
- **匯入**：使用套件匯入（`package:video_player_avfoundation/...`），不要用相對路徑
- **型別**：公開 API 需明確標註型別；區域變數可用 `var`/`final`
- **命名**：方法/變數用 `camelCase`，類別用 `PascalCase`

**必要的版權標頭：**
```dart
// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
```

### Objective-C
- **風格**：Google Objective-C 風格指南
- **格式化工具**：`clang-format`
- **前綴**：所有類別使用 `FVP` 前綴（FlutterVideoPlayer）
- **標頭檔**：公開標頭放在 `include/video_player_avfoundation/`
- **空值標註**：始終使用 `nullable`/`nonnull`
- **屬性**：使用 `nonatomic`，優先用 `strong` 而非 `retain`
- **KVO**：每個 `addObserver:` 必須配對 `removeObserver:`（記憶體安全關鍵）

### 錯誤處理
- 禁止使用型別抑制（`as any`、`@ts-ignore`）
- 原生端：使用 `FlutterError *` 傳遞錯誤
- Dart 端：拋出 `PlatformException` 處理平台錯誤
- 禁止空的 catch 區塊

## 架構

### 關鍵類別

| Dart | 用途 |
|------|------|
| `AVFoundationVideoPlayer` | 平台介面實作 |
| `messages.g.dart` | Pigeon 生成的 API |

| 原生 | 用途 |
|------|------|
| `FVPVideoPlayerPlugin` | 插件註冊、播放器工廠 |
| `FVPVideoPlayer` | 基礎播放器（支援平台視圖） |
| `FVPTextureBasedVideoPlayer` | 紋理式播放器（繼承 FVPVideoPlayer） |
| `FVPEventBridge` | 原生→Dart 事件通道 |
| `FVPFrameUpdater` | 紋理幀更新處理 |
| `FVPAVFactory` | AVFoundation 物件工廠（便於測試） |

### 設計模式
- **工廠模式**：`FVPAVFactory` 建立 AVFoundation 物件以利測試
- **協定式測試**：Mock 協定（`FVPDisplayLink`、`FVPViewProvider`）
- **Pigeon API**：`@HostApi()` 用於 Dart→原生；每個播放器的 `VideoPlayerInstanceApi` 帶通道後綴

### 平台特定程式碼

| 路徑 | 平台 |
|------|------|
| `darwin/video_player_avfoundation/Sources/video_player_avfoundation/` | 共用（iOS + macOS） |
| `darwin/.../video_player_avfoundation_ios/` | 僅 iOS |
| `darwin/.../video_player_avfoundation_macos/` | 僅 macOS |

**iOS 專用**：`CADisplayLink`、`AVAudioSession` 音訊混合
**macOS 專用**：`FVPCoreVideoDisplayLink`（macOS 14 以前）、無 `AVAudioSession`

## 測試模式

### Dart
```dart
@GenerateNiceMocks(<MockSpec<Object>>[
  MockSpec<AVFoundationVideoPlayerApi>(),
  MockSpec<VideoPlayerInstanceApi>(),
])
```

### 原生（XCTest）
- 使用 `OCMock` mock AVFoundation 物件
- 建立 stub 實作（`StubAVPlayer`、`StubFVPAVFactory`）
- 使用 `XCTestExpectation` 處理非同步操作

## 常見陷阱

1. **Pigeon 同步**：修改 `pigeons/messages.dart` 後必須重新生成
2. **KVO 清理**：每個 `addObserver:` 必須配對 `removeObserver:`，否則會崩潰
3. **Display Link 狀態**：必須正確處理播放和暫停兩種狀態
4. **影片合成**：僅支援檔案型媒體，不支援 HLS 串流
5. **音訊 Session（iOS）**：不要覆寫 `PlayAndRecord` 類別，其他插件依賴此設定

## 版本與更新日誌

**建議方式（自動化）：**
```bash
# 在儲存庫根目錄執行
dart run script/tool/bin/flutter_plugin_tools.dart update-release-info \
  --version=minimal \
  --base-branch=origin/main \
  --changelog="變更描述"
```

公開 API 變更用 `--version=minor`，錯誤修復用 `--version=minimal`。
