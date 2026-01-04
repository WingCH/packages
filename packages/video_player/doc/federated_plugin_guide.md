# Flutter Federated Plugin 開發實戰指南：從零開始

這份指南將教你如何運用 **Video Player** 的架構思維（Federated Plugin Architecture），從零開始設計並實作一個高品質的 Flutter Plugin。

---

## 第一部分：思考模式 (Mental Model)

在寫任何 Code 之前，你必須先調整思維。不要一開始就想「Android 怎麼寫、iOS 怎麼寫」，而是要先想「**Flutter 開發者想怎麼用**」。

### 核心三問：
1.  **功能定義**：我要提供什麼能力？(例：播放影片、取得電池電量、藍芽掃描)
2.  **介面設計 (The Contract)**：不管底層是誰，統一的溝通介面長怎樣？
3.  **平台差異**：哪些功能是所有平台都有的？哪些是特定平台才有的？

### 推薦流程：Interface First (介面優先)
不要先寫 Native Code。先定義好 `Platform Interface`，這樣你的架構才會乾淨，不會被特定平台的實作細節綁架。

---

## 第二部分：實戰步驟 (Step-by-Step)

假設我們要寫一個 **`awesome_player`**。

### Step 1: 建立專案結構 (Scaffolding)

你需要建立三個獨立的 Package（通常在一個 Monorepo 裡）：

```text
awesome_player/                  <-- App Facing Package (給人用)
  pubspec.yaml
awesome_player_platform_interface/  <-- Interface Package (定義標準)
  pubspec.yaml
awesome_player_android/          <-- Android Implementation (實作)
  pubspec.yaml
awesome_player_ios/              <-- iOS Implementation (實作)
  pubspec.yaml
```

### Step 2: 定義介面層 (Platform Interface)

這是最重要的一步。這裡定義了所有人都必須遵守的法律。

**位置**: `awesome_player_platform_interface/lib/awesome_player_platform_interface.dart`

```dart
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

abstract class AwesomePlayerPlatform extends PlatformInterface {
  // 建構子：驗證 Token 確保安全性
  AwesomePlayerPlatform() : super(token: _token);
  static final Object _token = Object();

  // 預設實作 (Singleton)
  static AwesomePlayerPlatform _instance = _PlaceholderImplementation();
  static AwesomePlayerPlatform get instance => _instance;

  // 讓外部註冊真正的實作 (比如 Android 版)
  static set instance(AwesomePlayerPlatform instance) {
    PlatformInterface.verify(instance, _token);
    _instance = instance;
  }

  // 定義方法：預設拋出 UnimplementedError
  Future<void> play() {
    throw UnimplementedError('play() has not been implemented.');
  }
}

class _PlaceholderImplementation extends AwesomePlayerPlatform {}
```

### Step 3: 設計 API 層 (App Facing Package)

這是開發者直接接觸的一層。它的工作是**轉發請求**。

**位置**: `awesome_player/lib/awesome_player.dart`

```dart
import 'package:awesome_player_platform_interface/awesome_player_platform_interface.dart';

class AwesomePlayer {
  // 直接呼叫 Interface 的單例
  Future<void> play() {
    return AwesomePlayerPlatform.instance.play();
  }
}
```

### Step 4: 實作原生層 & Pigeon (Platform Implementation)

現在我們要來做苦工了。我們用 Pigeon 來解決溝通問題。

#### 4.1 定義 Pigeon 文件
**位置**: `awesome_player_android/pigeons/messages.dart`

```dart
import 'package:pigeon/pigeon.dart';

@ConfigurePigeon(PigeonOptions(
  dartOut: 'lib/src/messages.g.dart',
  kotlinOut: 'android/src/main/kotlin/com/example/awesome_player/Messages.g.kt',
  kotlinOptions: KotlinOptions(package: 'com.example.awesome_player'),
))

@HostApi() // Flutter -> Native
abstract class AwesomePlayerApi {
  void play();
}
```

#### 4.2 生成程式碼
執行 Pigeon 指令生成 Dart 和 Kotlin 程式碼。

#### 4.3 實作 Dart 端的 Platform Class
**位置**: `awesome_player_android/lib/awesome_player_android.dart`

```dart
import 'package:awesome_player_platform_interface/awesome_player_platform_interface.dart';
import 'src/messages.g.dart'; // Pigeon 生成的

class AwesomePlayerAndroid extends AwesomePlayerPlatform {
  final AwesomePlayerApi _api = AwesomePlayerApi();

  // 註冊自己！告訴 Flutter 系統：「Android 平台請用我」
  static void registerWith() {
    AwesomePlayerPlatform.instance = AwesomePlayerAndroid();
  }

  @override
  Future<void> play() {
    // 呼叫 Pigeon 生成的 API
    return _api.play();
  }
}
```

#### 4.4 實作 Native 端的邏輯 (Kotlin)
**位置**: `android/src/main/kotlin/.../AwesomePlayerPlugin.kt`

```kotlin
class AwesomePlayerPlugin: FlutterPlugin, AwesomePlayerApi {
  override fun onAttachedToEngine(binding: FlutterPluginBinding) {
    // 綁定 Pigeon
    AwesomePlayerApi.setUp(binding.binaryMessenger, this)
  }

  override fun play() {
    // 這裡寫真正的 Android 播放邏輯
    player.play()
  }
}
```

### Step 5: 設定 `pubspec.yaml` (連結一切)

這步最容易忘記。你需要在 `pubspec.yaml` 告訴 Flutter 你的 Plugin 結構。

**檔案**: `awesome_player/pubspec.yaml`
```yaml
dependencies:
  awesome_player_platform_interface: ^1.0.0
  # 預設依賴這些實作，這樣使用者只要安載 awesome_player 就自動會有 Android/iOS 支援
  awesome_player_android: ^1.0.0
  awesome_player_ios: ^1.0.0
```

**檔案**: `awesome_player_android/pubspec.yaml`
```yaml
flutter:
  plugin:
    implements: awesome_player  # 我實作了 awesome_player
    platforms:
      android:
        package: com.example.awesome_player
        pluginClass: AwesomePlayerPlugin
        dartPluginClass: AwesomePlayerAndroid # 重要！指定 Dart 入口點
```

---

## 總結：開發順序檢核表

1.  ✅ **Interface**: 定義 `PlatformInterface` 抽象類別。
2.  ✅ **API**: 寫好 App Facing Package，呼叫 `PlatformInterface.instance`。
3.  ✅ **Pigeon**: 在實作層定義 `messages.dart` 並生成 Code。
4.  ✅ **Implementation (Dart)**: 繼承 `PlatformInterface`，用 Pigeon 呼叫 Native。
5.  ✅ **Implementation (Native)**: 實作 Pigeon 產生的 Interface。
6.  ✅ **Registration**: 在 `pubspec.yaml` 設定 `implements` 和 `dartPluginClass`。

只要跟著這個順序，你就能寫出結構清晰、易於擴充 (例如隨時加入 Windows/Web 支援) 的高品質 Flutter Plugin。
