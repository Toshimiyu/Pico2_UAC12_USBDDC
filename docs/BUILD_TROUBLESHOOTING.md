# Windows PowerShellでのビルドトラブルシュート

[English](BUILD_TROUBLESHOOTING_EN.md) | [日本語](BUILD_TROUBLESHOOTING.md)

## Windows PowerShellでのビルド

PowerShellで次のエラーが表示された場合：

```text
'cmake' is not recognized as an internal or external command
```

CMakeが利用可能か確認します。

```powershell
where.exe cmake
```

結果が表示されない場合は、CMakeをインストールしてください。

```powershell
winget install Kitware.CMake
```

インストール後、PowerShellを再起動します。CMakeがインストール済みでも見つからない場合は、インストール先をPATHに追加してください。

```powershell
$env:Path += ";C:\Program Files\CMake\bin"
```

恒久的に設定するには、Windowsのユーザー環境変数PATHにCMakeのインストール先を追加します。

インストールを確認します。

```powershell
cmake --version
```

バージョンが表示されたら、構成とビルドを実行します。

```powershell
cd C:\path\to\Pico2_UAC12_USBDDC

cmake -S . -B build -DUSB_AUDIO_CLASS_VERSION=1

cmake --build build
```

UAC2をビルドする場合は、`USB_AUDIO_CLASS_VERSION=1` を `USB_AUDIO_CLASS_VERSION=2` に変更してください。

SDK、コンパイラー、Ninjaに関するエラーが発生した場合は、Visual Studio CodeにRaspberry Pi Pico拡張機能をインストールし、次の環境を使用してください。

- Pico SDK v2.3.1
- Toolchain 15_2_Rel1
