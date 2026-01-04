# Flutter Video Player 架構世界觀

歡迎來到 `video_player` 的內部世界。你所看到的 "Interface"、"Native Code" 和 "Pigeon" 並非雜亂無章，而是遵循著一套嚴謹的 **Federated Plugin Architecture (聯邦式插件架構)**。

這個架構的核心目的是：**解耦 (Decoupling)**。它讓 App 開發者只需要面對一個統一的介面，而不需要關心底層是 Android 的 ExoPlayer 還是 iOS 的 AVFoundation。

## 1. 核心地圖：三大層級

試著想像這是一個跨國物流系統：

### 第一層：App Facing Package (門市櫃台)
*   **位置**: `video_player/`
*   **角色**: 這是你平時在 `pubspec.yaml` 裡引用的那個包。
*   **職責**:
    *   提供給開發者好用的 Widget (如 `VideoPlayer`) 和 Controller (如 `VideoPlayerController`)。
    *   它**完全不懂**如何播放影片，它只負責收單 (接收 `play()`, `pause()` 指令)。
    *   它會把訂單轉交給下一層。

### 第二層：Platform Interface (通用訂單格式)
*   **位置**: `video_player_platform_interface/`
*   **角色**: 這是整個架構的法律與契約。
*   **職責**:
    *   定義了一個抽象類別 `VideoPlayerPlatform`。
    *   規定了所有平台**必須**實作的方法 (例如 `init()`, `create()`, `play()`)。
    *   它不包含任何實作邏輯，只定義標準。這樣做的好處是，如果要新增 Windows 支援，只需要寫一個 Windows package 來實作這個介面，完全不需要動到第一層的程式碼。

### 第三層：Platform Implementation (在地物流中心)
*   **位置**:
    *   Android: `video_player_android/`
    *   iOS: `video_player_avfoundation/`
    *   Web: `video_player_web/`
*   **角色**: 真正的苦工，髒活都在這裡做。
*   **職責**:
    *   繼承並實作 `VideoPlayerPlatform`。
    *   **Android**: 呼叫 Google 的 `ExoPlayer`。
    *   **iOS**: 呼叫 Apple 的 `AVFoundation` (AVPlayer)。
    *   這層就是你看到 Native Code (Kotlin/Java, Swift/Obj-C) 出現的地方。

---

## 2. 神秘的信使：Pigeon (白鴿) 🐦

你提到的 **Pigeon** 是一個由 Flutter 官方開發的**程式碼產生器 (Code Generator)**。

### 為什麼需要它？
在沒有 Pigeon 的年代，Flutter 與 Native 溝通 (Platform Channel) 非常痛苦：
1.  **沒有型別安全 (Type Safety)**：你傳遞的是字串和 Map，寫錯一個 Key 程式就崩潰。
2.  **充滿 boilerplate**：你要在 Dart 寫一堆 `invokeMethod`，在 Kotlin/Swift 寫一堆 `if (call.method == "play")`。

### Pigeon 的角色
Pigeon 就像是一個**專業翻譯官**。
你只需要定義一份「中間語言」(Dart 檔案)，Pigeon 就會幫你自動生成 Dart 端和 Native 端的溝通程式碼。

### 在 video_player 中的實例
在 `video_player_android` 和 `video_player_avfoundation` 資料夾中，你都會看到 `pigeons/messages.dart`。這就是**翻譯對照表**。

**範例 (簡化版)**：
```dart
// pigeons/messages.dart

// 定義資料結構
class CreationOptions {
  String uri;
  Map<String, String> httpHeaders;
}

// 定義 API 介面 (Flutter 呼叫 Native)
@HostApi()
abstract class AndroidVideoPlayerApi {
  void initialize();
  int create(CreationOptions options);
  void play(int playerId);
  void pause(int playerId);
}
```

當你執行 Pigeon 生成指令後，它會自動產生：
1.  **Dart Code**: 幫你把 `play()` 封裝好，底層自動呼叫 `BinaryMessenger`。
2.  **Kotlin/Java Code**: 產生一個 Interface 讓你去實作，你只需要填入邏輯，不用管解析參數的髒活。
3.  **Obj-C/Swift Code**: 同上。

---

## 3. 完整的資料流 (Data Flow)

當你在 Flutter 程式碼中呼叫 `controller.play()` 時，發生了什麼事？

```mermaid
graph TD
    User[你的程式碼] -->|controller.play| Facade[video_player (Package)]
    Facade -->|VideoPlayerPlatform.instance.play| Interface[Platform Interface]
    Interface -->|delegate| Implementation[video_player_android / ios]
    
    subgraph "Pigeon 的魔法區域"
        Implementation -->|AndroidVideoPlayerApi.play| GenDart[Pigeon Generated Dart]
        GenDart -->|Binary Message| Channel[Method Channel]
        Channel -->|Binary Message| GenNative[Pigeon Generated Kotlin/ObjC]
    end
    
    GenNative -->|呼叫| NativeImpl[Messages.kt / messages.m]
    NativeImpl -->|操作| NativePlayer[ExoPlayer / AVPlayer]
```

## 4. 如何閱讀這份 Code？

如果你想學習 Native 架構，建議依照這個順序閱讀：

1.  **先看契約**: 打開 `video_player_platform_interface/lib/video_player_platform_interface.dart`，看看這個播放器到底支援哪些功能。
2.  **再看翻譯表**: 打開 `video_player_android/pigeons/messages.dart`，看看它如何定義資料結構和 API。
3.  **最後看實作**:
    *   **Android**: `video_player_android/android/src/main/kotlin/.../VideoPlayerPlugin.kt` (或是 `Messages.kt` 的實作部分)。
    *   **iOS**: `video_player_avfoundation/darwin/.../FVPVideoPlayerPlugin.m`。

## 總結
*   **Interface**: 定義標準，讓大家有規矩可循。
*   **Native Code**: 解決 Flutter 做不到的事 (直接操作硬體解碼器)。
*   **Pigeon**: 連結 Flutter 與 Native 的安全橋樑，消除人為失誤。
