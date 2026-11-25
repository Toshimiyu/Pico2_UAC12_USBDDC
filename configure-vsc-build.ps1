#
# VS Code 用 CMake 設定スクリプト
# 用途: VS Code で Compile Project タスク前に実行
# コマンド: cmake -S . -B build -DUSB_AUDIO_CLASS_VERSION=2
#

$repoRoot = Split-Path -Parent $PSScriptRoot

Write-Output "🔧 VS Code Build Configuration"
Write-Output "════════════════════════════════════════════"
Write-Output ""

# 環境変数設定（VS Code from path のため）
$env:PICO_SDK_PATH = "$env:USERPROFILE\.pico-sdk\sdk\2.3.1"
$env:PICO_TOOLCHAIN_PATH = "$env:USERPROFILE\.pico-sdk\toolchain\15_2_Rel1"
$env:PATH = "$env:USERPROFILE\.pico-sdk\toolchain\15_2_Rel1\bin;$env:USERPROFILE\.pico-sdk\ninja\v1.13.2;$env:USERPROFILE\.pico-sdk\cmake\v4.3.4\bin;$env:PATH"

Write-Output "✅ Environment variables set"
Write-Output ""

# ビルドディレクトリが存在したら削除（クリーンビルド）
if(Test-Path build) {
    Write-Output "🧹 Cleaning old build directory..."
    Remove-Item -Recurse -Force build
    Write-Output "   ✓ Old build removed"
} else {
    Write-Output "✓ No previous build to clean"
}

Write-Output ""
Write-Output "🔨 Configuring CMake..."
Write-Output "   Command: cmake -S . -B build -DUSB_AUDIO_CLASS_VERSION=2"
Write-Output ""

# CMake 設定
cmake -S . -B build -DUSB_AUDIO_CLASS_VERSION=2

if($LASTEXITCODE -ne 0) {
    Write-Output ""
    Write-Output "❌ CMake configuration failed!"
    exit 1
}

Write-Output ""
Write-Output "✅ CMake configuration complete"
Write-Output ""
Write-Output "次のステップ："
Write-Output "  • VS Code で Ctrl+Shift+B (Compile Project) を実行"
Write-Output "  • または: ninja -C build"
Write-Output ""
