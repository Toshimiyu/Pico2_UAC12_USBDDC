# Build Scripts Slimming Summary

## ✅ 実施完了内容

### Phase 1: 共通ライブラリ化（✅ 完了）

#### 新規作成ファイル
- **`tools/common-build.ps1`** （154 行）
  - 共通関数群を集約化
  - 重複コードを統一

#### 共通関数一覧

| 関数名 | 説明 | 利用スクリプト |
|--------|------|---------------|
| `Setup-BuildEnvironment` | PATH を CMake/Ninja/ARM ツールチェーン用に構成 | `build-level.ps1`, `build-all-levels.ps1` |
| `Resolve-PathSafe` | PowerShell パスを安全に解決（スペース・特文字対応） | `build-level.ps1`, `capture_cdc.ps1`, `capture_cdc_auto.ps1` |
| `New-LogDirectory` | ログディレクトリを作成（存在確認付き） | `build-level.ps1`, `capture_cdc_auto.ps1` |
| `Rotate-LogFile` | ログファイルをサイズ監視して `.bak` にアーカイブ | `capture_cdc_auto.ps1` |
| `Test-RequiredTools` | 必須ツールを PATH から検証 | `build-level.ps1` |
| `Save-BuildLog` | コンソール＋ファイルへの統一ログ出力 | `build-level.ps1` |

---

### 修正内容

#### 1. `build-level.ps1` ✅
**改善点**:
- ✅ 環境パス設定を `Setup-BuildEnvironment` に統一
- ✅ ツール検証を `Test-RequiredTools` に統一
- ✅ パス解決を `Resolve-PathSafe` に統一
- ✅ ログ出力を `Save-BuildLog` に統一
- ✅ **CMake 引数を配列形式で渡す**ことで確実な変数展開を実現
- ✅ PowerShell 7.x 対応（`finally` ブロック削除）
- ✅ コード行数: 209 行 → 237 行（機能追加による変更）

**Key Fix**:
```powershell
# Before: 変数が展開されずに "$Level" が CMakeCache に記録される
$cmakeOutput = & cmake -S . -B $OutputDir -G Ninja -DUSB_BRINGUP_LEVEL=$Level 2>&1

# After: 配列形式で確実に展開
$cmakeArgs = "-S", ".", "-B", $OutputDir, "-G", "Ninja", "-DUSB_BRINGUP_LEVEL=$Level"
$cmakeOutput = & cmake $cmakeArgs 2>&1
```

---

#### 2. `capture_cdc_auto.ps1` ✅
**改善点**:
- ✅ ログロールオーバー機能を追加（デフォルト 10 MB）
- ✅ パス解決を `Resolve-PathSafe` に統一
- ✅ ログディレクトリ作成を `New-LogDirectory` に統一
- ✅ `-LogMaxSizeBytes` パラメータで回転サイズをカスタマイズ可能

**使用例**:
```powershell
# デフォルト 10 MB で回転
.\tools\capture_cdc_auto.ps1 -Port COM3 -OutputDir logs

# カスタム: 5 MB で回転
.\tools\capture_cdc_auto.ps1 -Port COM3 -OutputDir logs -LogMaxSizeBytes (5 * 1MB)
```

---

#### 3. `capture_cdc.ps1` ✅
**改善点**:
- ✅ パス解決を `Resolve-PathSafe` に統一

---

#### 4. `build-all-levels.ps1` ✅
**改善点**:
- ✅ 共通ライブラリ読み込み追加（`common-build.ps1`）
- ✅ エラーハンドリングを統一

---

## 🧪 テスト結果

### ✅ 構文チェック
- `common-build.ps1`: ✓
- `build-level.ps1`: ✓
- `build-all-levels.ps1`: ✓
- `capture_cdc.ps1`: ✓
- `capture_cdc_auto.ps1`: ✓

### ✅ 機能テスト

**ログローテーション**:
- 5 MB ファイル → 3 MB 制限で自動 `.bak` アーカイブ成功 ✓
- 小さいログ → 回転しない（正常） ✓

**実ビルド（Level 2）**:
- CMake 設定: ✓ 成功
- Ninja ビルド: ✓ 成功（10.32s）
- ELF 生成: ✓ 成功（194,620 bytes text, 196,940 bytes bss）
- サイズ分析: ✓ 42.9% RAM使用率

---

## 📊 削減・統一の効果

| 指標 | 効果 |
|------|------|
| **重複コード削減** | 環境パス設定、パス解決処理、ログ出力が統一化 |
| **メンテナンス性向上** | 共通関数 1 箇所の修正が全スクリプトに反映 |
| **ログ肥大化対策** | 長時間実行でも `capture_events.log` が自動アーカイブ |
| **拡張性向上** | 新規スクリプト追加時に `common-build.ps1` を読み込むだけ |
| **信頼性向上** | 配列形式での引数渡しで変数展開を確実化 |

---

## 📁 ファイル一覧

```
tools/
├── common-build.ps1              （新規）共通ライブラリ ⭐
├── build-level.ps1               （修正）引数処理改善、共通関数統一化 ✅
├── build-all-levels.ps1          （修正）共通ライブラリ対応 ✅
├── capture_cdc.ps1               （修正）パス解決統一化 ✅
├── capture_cdc_auto.ps1          （修正）ログロールオーバー追加 ✅
└── analyze-builds.ps1            （既存、変更なし）
```

---

## 🚀 使用方法

### ビルドスクリプト（変更なし）
```powershell
# 通常ビルド
.\tools\build-level.ps1 -Level 2

# すべてのレベルをビルド
.\tools\build-all-levels.ps1

# ログ保存なし
.\tools\build-level.ps1 -Level 2 -NoLog
```

### キャプチャスクリプト（-LogMaxSizeBytes 追加）
```powershell
# デフォルト（10 MB で回転）
.\tools\capture_cdc_auto.ps1

# カスタムサイズ（5 MB で回転）
.\tools\capture_cdc_auto.ps1 -LogMaxSizeBytes (5 * 1MB)
```

---

## ✨ 主要な改善点サマリー

1. **共通化**: 5 つのスクリプトから重複コード を `common-build.ps1` に統合
2. **CMake 引数修正**: 配列形式での渡しで変数展開の確実性を向上
3. **ログ管理**: ロールオーバー機能で長時間実行時のディスク容量対策
4. **エラーハンドリング**: 統一的な例外処理で保守性向上
5. **PowerShell 7 対応**: 新版 PowerShell での互換性確保

---

---

## 📋 Build Configuration - Verified Facts (2026-11-25 22:24 JST)

### RAM Specification
- **Total Available**: 458,752 B (448 KiB)
- **Measurement Basis**: arm-none-eabi-size (BSD format: text, data, bss)

### All Levels - Clean Build Results

| Level | Config | Text | BSS | RAM % | Free | Build Time |
|-------|--------|------|-----|-------|------|-----------|
| **2** | FFT+Display (CDC OFF) | 194,620 B | 196,940 B | 42.9% | 261,812 B | 10.33 s |
| **3** | CDC Diagnostic (FFT OFF) | 121,864 B | 113,328 B | 24.7% | 345,424 B | 10.35 s |
| **4** | CDC+FFT+Display | 211,608 B | 242,268 B | 52.8% | 216,484 B | 10.40 s |

### Key Findings

1. **EVT_LOG_N Configuration Impact**
   - Source: `src/usb_device_control.c` (lines 83-87)
   - Level 3 (CDC ON): EVT_LOG_N = 128 → evt_log array = 1,536 B total
   - Level 2 (CDC OFF): EVT_LOG_N = 64 → evt_log array = 768 B total
   - Level 3 previous measurement (114,864 B using EVT_LOG_N=256) → current 113,328 B matches array reduction

2. **Feature Compilation**
   - `USB_CDC_ENABLED = (USB_BRINGUP_LEVEL >= 3)`
   - `USB_FFT_ENABLED = (USB_BRINGUP_LEVEL != 3)`
   - Verified: Level definitions match actual BSS impact

3. **Memory Headroom**
   - Level 2: Safe margin (57.1% free)
   - Level 3: Safe margin (75.3% free)
   - Level 4: Operational (47.2% free) - monitor heap during CDC logging

### Build Consistency
- CMake configuration: ✅ Applied correct USB_BRINGUP_LEVEL for each build
- Linking: ✅ All 116 targets resolved without errors
- Size analysis: ✅ Consistent BSS calculation across all levels

**Verification Date**: 2026-11-25 22:24 JST  
**Analysis Method**: arm-none-eabi-size + CMakeCache.txt inspection + source code review

---

**実施日**: 2026-11-25 22:18 JST（スクリプト整備完了）  
**検証日**: 2026-11-25 22:24 JST（全レベルビルド・RAM分析完了）  
**状態**: ✅ **すべて完了・テスト合格・事実確認完了**

