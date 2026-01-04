# video_player_avfoundation PIP 實作指南

本文檔詳細解釋 `video_player_avfoundation` 的影片播放流程，以及如何加入 Picture-in-Picture (PIP) 支援。

---

## 第一部分：現有影片播放流程

### 1. Dart 端發起創建請求

```
用戶 App → VideoPlayerController → AVFoundationVideoPlayer → Pigeon API → 原生端
```

當你在 Dart 調用 `VideoPlayerController.network(url)` 時：

```dart
// avfoundation_video_player.dart
Future<int?> createWithOptions(VideoCreationOptions options) async {
  // 透過 Pigeon 調用原生端
  final int playerId = await _api.createForTextureView(creationOptions);
  // 或者
  final int playerId = await _api.createForPlatformView(creationOptions);
}
```

### 2. 原生端創建播放器

```objc
// FVPVideoPlayerPlugin.m
- (FVPTexturePlayerIds *)createTexturePlayerWithOptions:(FVPCreationOptions *)options {
  // 1. 創建 AVPlayerItem（包含影片 URL）
  AVPlayerItem *item = [self playerItemWithCreationOptions:options];
  
  // 2. 創建播放器實例
  FVPTextureBasedVideoPlayer *player = [[FVPTextureBasedVideoPlayer alloc] 
      initWithPlayerItem:item ...];
  
  // 3. 註冊到 Flutter texture registry
  int64_t textureId = [self.registrar.textures registerTexture:player];
}
```

### 3. 播放器內部結構

```
FVPVideoPlayer（基類）
    ├── AVPlayer ─────────────────→ 負責解碼和播放
    ├── AVPlayerItem ─────────────→ 影片資源
    ├── AVPlayerItemVideoOutput ──→ 提取像素數據
    └── eventListener ────────────→ 回傳事件給 Dart

FVPTextureBasedVideoPlayer（繼承 FVPVideoPlayer）
    ├── 繼承以上所有
    ├── AVPlayerLayer（隱藏）─────→ 修復 iOS 16 加密影片 bug
    ├── FVPFrameUpdater ─────────→ 通知 Flutter engine 有新 frame
    └── displayLink ─────────────→ 同步螢幕刷新率
```

### 4. 兩種渲染模式

#### Texture 模式（預設）

```
AVPlayer 解碼 → AVPlayerItemVideoOutput 提取 pixel buffer 
    → Flutter Engine 透過 copyPixelBuffer 取得 
    → 用 Texture widget 渲染
```

**特點**：Flutter 完全控制渲染，可以加動畫、transform 等

#### Platform View 模式

```
AVPlayer 解碼 → AVPlayerLayer 直接渲染到 UIView
    → 用 UiKitView 嵌入 Flutter
```

**特點**：原生渲染，性能較好，但 Flutter 難以控制

### 5. 事件回傳流程

```
AVPlayer KVO 觀察 → FVPVideoPlayer 接收 
    → FVPEventBridge（FlutterEventChannel）
    → Dart videoEventsFor() Stream
```

---

## 第二部分：為什麼不支援 PIP？

### PIP 的技術需求

根據 Apple 官方文檔，`AVPictureInPictureController` 需要：

```objc
// 必須有 AVPlayerLayer
AVPictureInPictureController *pipController = 
    [[AVPictureInPictureController alloc] initWithPlayerLayer:playerLayer];
```

### 問題 1：AVPlayerLayer 存在但未暴露

```objc
// FVPTextureBasedVideoPlayer.m
_playerLayer = [AVPlayerLayer playerLayerWithPlayer:self.player];
[viewProvider.view.layer addSublayer:self.playerLayer];  // 只是加了做 sublayer
// 但是沒有任何 PIP 相關的處理！
```

這個 `playerLayer` 是為了修復 iOS 16 加密影片和 aspect ratio bug 而加的，**不是為了 PIP**。

### 問題 2：沒有 AVPictureInPictureController

```bash
grep -r "AVPictureInPictureController" .
# 結果：沒有任何匹配
```

整個 codebase 根本沒有創建過 PIP controller。

### 問題 3：沒有 PIP 相關 API

```dart
// messages.dart - 現有 API
abstract class VideoPlayerInstanceApi {
  void setLooping(bool looping);
  void setVolume(double volume);
  void play();
  void pause();
  // ... 沒有任何 PIP 方法
}
```

### 問題 4：沒有 PIP 事件回傳機制

```objc
// FVPVideoEventListener.h - 現有事件
@protocol FVPVideoEventListener <NSObject>
- (void)videoPlayerDidInitializeWithDuration:size:;
- (void)videoPlayerDidComplete;
- (void)videoPlayerDidStartBuffering;
// ... 沒有 PIP 相關事件
@end
```

### 問題 5：Info.plist 配置

PIP 需要 Background Audio capability。雖然 plugin 有設定 `AVAudioSessionCategoryPlayback`，但用戶的 app 可能沒有在 Info.plist 加 background mode。

---

## 第三部分：如何支援 PIP

### Step 1: 修改 Pigeon API 定義

```dart
// pigeons/messages.dart
@HostApi()
abstract class AVFoundationVideoPlayerApi {
  // 新增：檢查設備是否支援 PIP
  bool isPictureInPictureSupported();
}

@HostApi()
abstract class VideoPlayerInstanceApi {
  // 新增：PIP 控制
  bool isPictureInPicturePossible();
  bool isPictureInPictureActive();
  void startPictureInPicture();
  void stopPictureInPicture();
}
```

然後執行：

```bash
dart run pigeon --input pigeons/messages.dart
```

### Step 2: 在 FVPVideoPlayer 加入 PIP Controller

```objc
// FVPVideoPlayer.h
#import <AVKit/AVKit.h>

@interface FVPVideoPlayer : NSObject <AVPictureInPictureControllerDelegate>
@property(nonatomic, strong) AVPictureInPictureController *pipController;
@property(nonatomic, strong) AVPlayerLayer *playerLayer;  // 新增
@end
```

### Step 3: 初始化 PIP Controller

```objc
// FVPVideoPlayer.m
- (void)setupPictureInPictureWithPlayerLayer:(AVPlayerLayer *)layer {
  if (![AVPictureInPictureController isPictureInPictureSupported]) {
    return;
  }
  
  _playerLayer = layer;
  _pipController = [[AVPictureInPictureController alloc] initWithPlayerLayer:layer];
  _pipController.delegate = self;
}
```

### Step 4: 實作 PIP Delegate

```objc
// FVPVideoPlayer.m
#pragma mark - AVPictureInPictureControllerDelegate

- (void)pictureInPictureControllerWillStartPictureInPicture:
    (AVPictureInPictureController *)pictureInPictureController {
  [self.eventListener videoPlayerWillStartPictureInPicture];
}

- (void)pictureInPictureControllerDidStartPictureInPicture:
    (AVPictureInPictureController *)pictureInPictureController {
  [self.eventListener videoPlayerDidStartPictureInPicture];
}

- (void)pictureInPictureControllerWillStopPictureInPicture:
    (AVPictureInPictureController *)pictureInPictureController {
  [self.eventListener videoPlayerWillStopPictureInPicture];
}

- (void)pictureInPictureControllerDidStopPictureInPicture:
    (AVPictureInPictureController *)pictureInPictureController {
  [self.eventListener videoPlayerDidStopPictureInPicture];
}

- (void)pictureInPictureController:(AVPictureInPictureController *)pictureInPictureController
    restoreUserInterfaceForPictureInPictureStopWithCompletionHandler:
        (void (^)(BOOL))completionHandler {
  [self.eventListener videoPlayerRestoreUserInterfaceForPictureInPicture];
  completionHandler(YES);
}
```

### Step 5: 擴展 Event Listener

```objc
// FVPVideoEventListener.h
@protocol FVPVideoEventListener <NSObject>
// 現有事件...

// 新增 PIP 事件
- (void)videoPlayerWillStartPictureInPicture;
- (void)videoPlayerDidStartPictureInPicture;
- (void)videoPlayerWillStopPictureInPicture;
- (void)videoPlayerDidStopPictureInPicture;
- (void)videoPlayerRestoreUserInterfaceForPictureInPicture;
@end
```

### Step 6: 修改 FVPTextureBasedVideoPlayer

```objc
// FVPTextureBasedVideoPlayer.m
- (instancetype)initWithPlayerItem:(AVPlayerItem *)item
                      frameUpdater:(FVPFrameUpdater *)frameUpdater
                       displayLink:(NSObject<FVPDisplayLink> *)displayLink
                         avFactory:(id<FVPAVFactory>)avFactory
                      viewProvider:(NSObject<FVPViewProvider> *)viewProvider {
  self = [super initWithPlayerItem:item avFactory:avFactory viewProvider:viewProvider];
  if (self) {
    // 現有代碼...
    _playerLayer = [AVPlayerLayer playerLayerWithPlayer:self.player];
    [viewProvider.view.layer addSublayer:self.playerLayer];
    
    // 新增：設置 PIP
    [self setupPictureInPictureWithPlayerLayer:_playerLayer];
  }
  return self;
}
```

### Step 7: Dart 端實作

```dart
// avfoundation_video_player.dart
class AVFoundationVideoPlayer extends VideoPlayerPlatform {
  // 新增 PIP 方法
  
  @override
  Future<bool> isPictureInPictureSupported() {
    return _api.isPictureInPictureSupported();
  }
  
  @override
  Future<void> startPictureInPicture(int playerId) {
    return _playerWith(id: playerId).startPictureInPicture();
  }
  
  @override
  Future<void> stopPictureInPicture(int playerId) {
    return _playerWith(id: playerId).stopPictureInPicture();
  }
  
  @override
  Future<bool> isPictureInPictureActive(int playerId) {
    return _playerWith(id: playerId).isPictureInPictureActive();
  }

  // 新增：監聽 PIP 狀態流（非 Pigeon API，僅 Dart 端實作）
  Stream<bool> pipStatusStream(int playerId) {
    return _pipStreamControllers[playerId]?.stream ?? const Stream.empty();
  }
}
```

### Step 8: 處理 Event Channel

```dart
// avfoundation_video_player.dart - videoEventsFor()
VideoEvent _eventFromMap(Map<dynamic, dynamic> map) {
  return switch (map['event']) {
    'initialized' => VideoEvent(
        eventType: VideoEventType.initialized,
        duration: Duration(milliseconds: map['duration'] as int),
        size: Size(
          (map['width'] as num).toDouble(),
          (map['height'] as num).toDouble(),
        ),
      ),
    'completed' => VideoEvent(eventType: VideoEventType.completed),
    'bufferingStart' => VideoEvent(eventType: VideoEventType.bufferingStart),
    'bufferingEnd' => VideoEvent(eventType: VideoEventType.bufferingEnd),
    // 新增 PIP 事件（目前映射到 unknown，待 platform_interface 更新）
    'pipWillStart' => VideoEvent(eventType: VideoEventType.unknown),
    'pipStarted' => VideoEvent(eventType: VideoEventType.unknown),
    'pipWillStop' => VideoEvent(eventType: VideoEventType.unknown),
    'pipStopped' => VideoEvent(eventType: VideoEventType.unknown),
    'pipRestoreUserInterface' => VideoEvent(eventType: VideoEventType.unknown),
    _ => VideoEvent(eventType: VideoEventType.unknown),
  };
}
```

> **注意**：PIP 事件目前映射到 `VideoEventType.unknown`，因為 `video_player_platform_interface` 
> 尚未定義 PIP 相關的 `VideoEventType`。若需要在 Dart 端處理 PIP 事件，需要先更新 platform interface。

---

## 架構流程圖

### 現狀（無 PIP）

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Dart App  │ ──→ │  AVPlayer   │ ──→ │   Texture   │
└─────────────┘     │  (解碼)      │     │  (渲染)     │
                    └─────────────┘     └─────────────┘
                           ↓
                    AVPlayerLayer（隱藏，只修 bug）
                           ✗ 無 PIP Controller
```

### 加入 PIP 後

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Dart App  │ ──→ │  AVPlayer   │ ──→ │   Texture   │
└─────────────┘     │  (解碼)      │     │  (渲染)     │
       ↓            └─────────────┘     └─────────────┘
  startPIP()               ↓
       ↓            AVPlayerLayer
       ↓                   ↓
       └──────────→ AVPictureInPictureController
                           ↓
                    ┌─────────────┐
                    │  PIP 視窗   │ ← 系統控制
                    └─────────────┘
```

---

## 需要修改的檔案清單

| 檔案 | 修改內容 |
|------|----------|
| `pigeons/messages.dart` | 新增 PIP API 定義 |
| `lib/src/messages.g.dart` | Pigeon 自動生成 |
| `lib/src/avfoundation_video_player.dart` | Dart 端 PIP API 實作 |
| `darwin/.../messages.g.h` | Pigeon 自動生成 |
| `darwin/.../messages.g.m` | Pigeon 自動生成 |
| `darwin/.../FVPVideoPlayer.h` | 新增 PIP controller 屬性、delegate 協定、AVKit import |
| `darwin/.../FVPVideoPlayer_Internal.h` | 新增 `pipController` 和 `pipPlayerLayer` 內部屬性 |
| `darwin/.../FVPVideoPlayer.m` | 實作 PIP controller 初始化和 delegate 方法 |
| `darwin/.../FVPVideoPlayerPlugin.m` | 實作全域 `isPictureInPictureSupported` 方法 |
| `darwin/.../FVPTextureBasedVideoPlayer.m` | 調用 `setupPictureInPictureWithPlayerLayer:` |
| `darwin/.../FVPNativeVideoViewFactory.m` | Platform View 模式下設置 PIP |
| `darwin/.../FVPVideoEventListener.h` | 新增 5 個 PIP 事件回調定義 |
| `darwin/.../FVPEventBridge.m` | 實作 PIP 事件發送到 Dart |

---

## 用戶端配置需求

使用 PIP 功能的 App 需要在 `Info.plist` 加入：

```xml
<key>UIBackgroundModes</key>
<array>
    <string>audio</string>
</array>
```

以及在 Xcode 中啟用 "Background Modes" capability 並勾選 "Audio, AirPlay, and Picture in Picture"。

---

## 參考資料

- [Apple Developer: Adopting Picture in Picture in a Custom Player](https://developer.apple.com/documentation/avkit/adopting-picture-in-picture-in-a-custom-player)
- [AVPictureInPictureController Documentation](https://developer.apple.com/documentation/avkit/avpictureinpicturecontroller)
- [AVPictureInPictureControllerDelegate](https://developer.apple.com/documentation/avkit/avpictureinpicturecontrollerdelegate)

---

## 實作狀態

✅ **已完成**（2025-01-04）

### 已實作的功能

| 功能 | 狀態 | 備註 |
|------|------|------|
| `isPictureInPictureSupported()` | ✅ | 全域 API，檢查設備支援 |
| `isPictureInPicturePossible(playerId)` | ✅ | 檢查當前播放器是否可啟動 PIP |
| `isPictureInPictureActive(playerId)` | ✅ | 檢查 PIP 是否正在運行 |
| `startPictureInPicture(playerId)` | ✅ | 啟動 PIP |
| `stopPictureInPicture(playerId)` | ✅ | 停止 PIP |
| PIP 事件回調 | ✅ | 5 個事件：willStart, started, willStop, stopped, restoreUI |
| Texture 模式支援 | ✅ | 透過 FVPTextureBasedVideoPlayer |
| Platform View 模式支援 | ✅ | 透過 FVPNativeVideoViewFactory |
| 自動 PIP（進入背景時） | ✅ | iOS 14.2+ 支援 |

### 驗證結果

- ✅ `flutter analyze` - No issues found
- ✅ `flutter test` - 23 tests passed

### 待辦事項

- [ ] 在 Xcode 中執行 XCTest 確認原生端編譯正確
- [ ] 在真實 iOS 設備上測試 PIP 功能
- [ ] 更新 `video_player_platform_interface` 新增 PIP 相關 `VideoEventType`
- [ ] 更新 CHANGELOG.md 和 pubspec.yaml 版本號

---

## 兩種渲染模式的 PIP 差異（技術深入）

### 概述

`video_player_avfoundation` 提供兩種渲染模式，它們在 PIP 實作上有根本性的差異：

| | Texture 模式（預設） | Platform View 模式 |
|---|---|---|
| Dart 使用方式 | 預設 | `useNativeVideoView: true` |
| 影片渲染者 | Flutter Texture | AVPlayerLayer |
| AVPlayerLayer 角色 | 隱藏輔助層 | 主要渲染層 |
| PIP 過渡效果 | 需要特殊處理 | 無縫過渡 |

### 渲染架構圖

#### Texture 模式

```
┌─────────────────────────────────────────────────────────────────┐
│                        Flutter App                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   ┌────────────────────┐      ┌────────────────────┐            │
│   │   Texture Widget   │ ←──  │  copyPixelBuffer   │            │
│   │   （顯示影片）       │      │  （每幀提取像素）    │            │
│   └────────────────────┘      └────────────────────┘            │
│            ↑                            ↑                        │
│            │                            │                        │
│   ┌────────┴───────────────────────────┴────────┐               │
│   │           AVPlayerItemVideoOutput            │               │
│   └─────────────────────┬───────────────────────┘               │
│                         │                                        │
│   ┌─────────────────────┴───────────────────────┐               │
│   │                  AVPlayer                    │               │
│   │               （解碼影片）                    │               │
│   └─────────────────────┬───────────────────────┘               │
│                         │                                        │
│   ┌─────────────────────┴───────────────────────┐               │
│   │    AVPlayerLayer（隱藏，放在螢幕外）          │               │
│   │    位置：(-width, -height)                   │               │
│   │    尺寸：影片實際尺寸                         │  ← PIP 來源   │
│   └─────────────────────────────────────────────┘               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ↓ 進入 PIP
                    ┌─────────────────┐
                    │    PIP 視窗     │
                    │  （系統控制）    │
                    └─────────────────┘
```

**為什麼 Texture 模式需要隱藏的 AVPlayerLayer？**

1. 原本用於修復 iOS 16 加密影片 bug（#111457）
2. 修復某些影片寬高比錯誤（#109116）
3. PIP 必須使用 AVPlayerLayer，不能直接用 pixel buffer

#### Platform View 模式

```
┌─────────────────────────────────────────────────────────────────┐
│                        Flutter App                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   ┌────────────────────────────────────────────┐                │
│   │              UiKitView Widget               │                │
│   │         （嵌入原生 UIView）                  │                │
│   └────────────────────┬───────────────────────┘                │
│                        │                                         │
│   ┌────────────────────┴───────────────────────┐                │
│   │           FVPPlayerView (UIView)            │                │
│   │   ┌───────────────────────────────────┐    │                │
│   │   │     AVPlayerLayer（直接可見）       │    │  ← PIP 來源   │
│   │   │     （顯示影片）                    │    │                │
│   │   └───────────────────────────────────┘    │                │
│   └────────────────────┬───────────────────────┘                │
│                        │                                         │
│   ┌────────────────────┴───────────────────────┐                │
│   │                  AVPlayer                   │                │
│   │               （解碼影片）                   │                │
│   └─────────────────────────────────────────────┘                │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ↓ 進入 PIP（無縫過渡）
                    ┌─────────────────┐
                    │    PIP 視窗     │
                    │  （系統控制）    │
                    └─────────────────┘
```

### PIP 過渡差異

#### Platform View：無縫過渡

```
時間軸 ────────────────────────────────────────────→

[App 可見]                    [進入背景]           [PIP 顯示]
     │                             │                    │
     ▼                             ▼                    ▼
┌──────────┐                 ┌──────────┐         ┌──────────┐
│          │                 │          │         │          │
│  影片    │  ─── 直接 ───→  │  過渡中  │  ───→  │   PIP    │
│ (可見)   │      過渡       │          │         │  視窗    │
│          │                 │          │         │          │
└──────────┘                 └──────────┘         └──────────┘

AVPlayerLayer 已經可見 → 系統直接縮小動畫 → 無延遲
```

#### Texture 模式（優化前）：明顯延遲

```
時間軸 ────────────────────────────────────────────────────→

[App 可見]              [進入背景]        [等待...]      [PIP 顯示]
     │                       │                │              │
     ▼                       ▼                ▼              ▼
┌──────────┐           ┌──────────┐     ┌──────────┐   ┌──────────┐
│          │           │          │     │  調整    │   │          │
│  Texture │  ───→     │  Layer   │ ──→ │  尺寸    │ → │   PIP    │
│ (可見)   │           │  1x1     │     │  開始    │   │  視窗    │
│          │           │ (隱藏)   │     │  渲染    │   │          │
└──────────┘           └──────────┘     └──────────┘   └──────────┘

AVPlayerLayer 是 1x1 像素 → 系統需要調整尺寸 → 有延遲
```

#### Texture 模式（優化後）：流暢過渡

```
時間軸 ────────────────────────────────────────────→

[影片載入完成時]
     │
     ▼
AVPlayerLayer.frame = CGRect(-width, -height, width, height)
     │
     │  （預先設置為影片實際尺寸，放在螢幕外）
     │
     ▼

[App 可見]              [進入背景]           [PIP 顯示]
     │                       │                    │
     ▼                       ▼                    ▼
┌──────────┐           ┌──────────┐         ┌──────────┐
│          │           │  Layer   │         │          │
│  Texture │  ───→     │ 正確尺寸 │  ───→   │   PIP    │
│ (可見)   │           │ (隱藏)   │         │  視窗    │
│          │           │          │         │          │
└──────────┘           └──────────┘         └──────────┘

AVPlayerLayer 已有正確尺寸 → 系統直接使用 → 延遲大幅減少
```

### 關鍵程式碼

#### 1. 初始化時設置最小 frame（FVPTextureBasedVideoPlayer.m）

```objc
// 初始化時先設置 1x1，確保 layer 存在
_playerLayer = [AVPlayerLayer playerLayerWithPlayer:self.player];
#if TARGET_OS_IOS
// 最小 frame，放在螢幕外避免視覺干擾
_playerLayer.frame = CGRectMake(-1, -1, 1, 1);
#endif
[viewProvider.view.layer addSublayer:self.playerLayer];

#if TARGET_OS_IOS
[self setupPictureInPictureWithPlayerLayer:_playerLayer];
#endif
```

#### 2. 影片載入後更新為實際尺寸（FVPTextureBasedVideoPlayer.m）

```objc
- (void)reportInitialized {
  [super reportInitialized];

#if TARGET_OS_IOS
  // 影片載入完成後，更新 playerLayer 為實際尺寸
  // 這樣 PIP 啟動時不需要調整尺寸，過渡更流暢
  CGSize videoSize = self.player.currentItem.presentationSize;
  if (videoSize.width > 0 && videoSize.height > 0) {
    // 放在螢幕外但使用正確尺寸
    _playerLayer.frame = CGRectMake(-videoSize.width, -videoSize.height, 
                                     videoSize.width, videoSize.height);
  }
#endif
}
```

#### 3. 啟用自動 PIP（FVPVideoPlayer.m）

```objc
- (void)setupPictureInPictureWithPlayerLayer:(AVPlayerLayer *)playerLayer {
  if (![AVPictureInPictureController isPictureInPictureSupported]) {
    return;
  }

  _pipPlayerLayer = playerLayer;
  _pipController = [[AVPictureInPictureController alloc] initWithPlayerLayer:playerLayer];
  _pipController.delegate = self;

  // iOS 14.2+ 支援自動 PIP：影片播放中進入背景時自動啟動
  if (@available(iOS 14.2, *)) {
    _pipController.canStartPictureInPictureAutomaticallyFromInline = YES;
  }
}
```

### 系統需求

| 功能 | 最低版本 | 備註 |
|------|---------|------|
| 基本 PIP | iOS 14.0 | `AVPictureInPictureController` |
| 自動 PIP | iOS 14.2 | `canStartPictureInPictureAutomaticallyFromInline` |
| 模擬器支援 | ❌ | 僅支援真實設備 |

### 故障排除

| 問題 | 可能原因 | 解決方案 |
|------|---------|----------|
| PIP 按鈕不出現 | `isPictureInPicturePossible` 為 false | 確認影片已載入並開始播放 |
| 進入背景沒有自動 PIP | iOS < 14.2 或影片未播放 | 升級 iOS 或確認影片正在播放 |
| PIP 視窗黑屏 | AVPlayerLayer frame 為零 | 確認 `reportInitialized` 有正確設置 frame |
| Texture 模式 PIP 有延遲 | frame 更新時機問題 | 檢查 `reportInitialized` 是否被正確調用 |
| Info.plist 配置缺失 | 缺少背景模式 | 加入 `UIBackgroundModes: audio` |

### 選擇建議

| 使用場景 | 推薦模式 | 原因 |
|---------|---------|------|
| 重視 PIP 體驗 | Platform View | 無縫過渡，最佳視覺效果 |
| 需要 Flutter 動畫/特效 | Texture | 可對影片套用 Flutter transform |
| 一般播放 | Texture（預設） | 更好的 Flutter 整合 |
