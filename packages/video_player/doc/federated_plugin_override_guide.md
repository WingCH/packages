# Flutter Federated Plugin 進階指南：如何「騎劫」官方實作？

如果你不滿意官方 `video_player` 的 iOS 實作（例如想換成 VLC，或是有特定 Bug 修不好），你完全不需要 Fork 整個專案。

根據 **Federated Plugin Architecture**，你可以利用 **「Platform Implementation Override」 (平台實作覆蓋)** 技術，無痛換掉底層引擎。

以下是具體的操作步驟：

---

## 步驟 1：建立你的 Custom Package

你需要建立一個新的 Flutter Package（假設叫 `my_super_player_ios`）。

**`my_super_player_ios/pubspec.yaml`**:
```yaml
name: my_super_player_ios
version: 0.0.1

environment:
  sdk: ">=3.0.0 <4.0.0"
  flutter: ">=3.3.0"

dependencies:
  flutter:
    sdk: flutter
  # 關鍵：必須依賴這個介面包
  video_player_platform_interface: ^6.0.0 

flutter:
  plugin:
    implements: video_player # 宣告：我是 video_player 的實作！
    platforms:
      ios:
        pluginClass: MySuperPlayerPlugin
        dartPluginClass: MySuperPlayerIOS # 這是 Dart 入口點
```

## 步驟 2：實作 Dart 端的 Platform Class

在你的新 Package 裡，你需要繼承 `VideoPlayerPlatform`。

**`my_super_player_ios/lib/my_super_player_ios.dart`**:
```dart
import 'package:flutter/foundation.dart';
import 'package:video_player_platform_interface/video_player_platform_interface.dart';

class MySuperPlayerIOS extends VideoPlayerPlatform {
  // 註冊方法
  static void registerWith() {
    VideoPlayerPlatform.instance = MySuperPlayerIOS();
  }

  /// 實作必須的方法
  @override
  Future<void> init() async {
    // 呼叫你自己寫的 Native Code
    // 你可以用 MethodChannel，或者用 Pigeon (推薦複製官方的 messages.dart 來改)
    print("哈哈！我是被騎劫後的 iOS Player 初始化！");
  }

  @override
  Future<int?> create(DataSource dataSource) async {
    // 你的實作邏輯...
    return 123; 
  }
  
  // ... 實作其他所有 required 的方法
}
```

## 步驟 3：實作 iOS Native Code

這部分就是你「不滿意」原本實作的地方。你可以用 Swift 或 Objective-C 重寫。

*   **Tip**: 如果你想省事，建議去 `video_player_avfoundation` 複製它的 `pigeons/messages.dart` 到你的專案。這樣你的 Dart 端和 Native 端溝通協議就跟官方一模一樣，你只需要專注在 Native 端的 `AVPlayer` 邏輯修改。

## 步驟 4：執行「騎劫」 (The Switch)

這一步最關鍵。你需要決定用哪種方式讓 App 使用你的實作。這裡推薦兩種最可靠的方法：

### 方法 A：依賴覆蓋 (Dependency Override) - 偷天換日法 🥷

這個方法的原理是**完全取代**官方包，讓官方實作連被下載的機會都沒有。

1.  將你的 package `pubspec.yaml` 中的 `name` 改為 `video_player_avfoundation` (即假冒官方包名)。
2.  在 App 的 `pubspec.yaml` 使用 `dependency_overrides`：

```yaml
dependencies:
  video_player: ^2.8.0

dependency_overrides:
  # 當任何人（包括 video_player）想找 'video_player_avfoundation' 時
  # 強制指去我自己寫的 package！
  video_player_avfoundation:
    path: ./packages/my_super_player_ios 
```

**優點**：隱形替換，App 代碼完全不用改。
**缺點**：你需要改 package name，且維護上可能會混淆。

### 方法 B：手動強制覆蓋 (Manual Override) - 導演指定法 🎬 (**推薦**)

這是最乾淨俐落的方法。雖然兩個實作都存在，但在程式啟動時，你利用 Dart 程式碼強制指定「麥克風交給誰」。

在你的 App 的 `main.dart` 裡面：

```dart
import 'package:flutter/material.dart';
import 'package:video_player/video_player.dart';
// 引入你的包
import 'package:my_super_player_ios/my_super_player_ios.dart'; 

void main() {
  // 1. 確保 Flutter Engine 初始化
  WidgetsFlutterBinding.ensureInitialized();

  // 2. 【騎劫開始】強制指定 Platform Instance
  // 即使 video_player 預設載入了官方實作，這行程式碼會把它覆蓋掉
  // 從此以後，VideoPlayerController 呼叫的每一個指令，都會轉發給你的 MySuperPlayerIOS
  VideoPlayerPlatform.instance = MySuperPlayerIOS();

  // 3. 啟動 App
  runApp(const MyApp());
}
```

**優點**：邏輯清晰，不需要改 package name，隨時可以切換回去。

## 結果會發生什麼事？

當你在 UI 寫：
```dart
VideoPlayerController.network('https://...')
```

它的執行路徑會變成：
1.  `VideoPlayerController` (官方 UI)
2.  呼叫 `VideoPlayerPlatform.instance`
3.  **導向 `MySuperPlayerIOS` (你的實作)**
4.  呼叫你的 iOS Native Code

這就是所謂的 **Dependency Injection (依賴注入)** 模式在 Flutter Plugin 系統的體現。你不需要改動官方的一行程式碼，就能夠把核心引擎換掉。
