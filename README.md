# nesDS (Mapper 95 Support Fork)

A custom fork of **NesDS 2.1** (`DS-Homebrew/NesDS`) adding **iNES Mapper 95 (NAMCOT-3425)** support (*Dragon Buster*, etc.), FDS safety guards, improved configuration/BIOS search paths, enhanced cheat management, and various stability fixes.

ナムコ **Mapper 95**（『ドラゴンバスター』等）への新規対応に加え、FDS（ディスクシステム）の安全性向上、設定ファイルやチート機能の使い勝手、操作性・音声周りの不具合を改善した `nesDS` のカスタムビルドです。

---

## Key Features & Improvements (English)

### 1. Mapper 95 Support (*Dragon Buster*, etc.)
* **iNES Mapper 95 (NAMCOT-3425 / MMC3 variant)**: Implemented CHR banking and CHR A15-controlled nametable mirroring, allowing Mapper 95 games such as *Dragon Buster* to run properly.
* **Clean Rendering Defaults**: Optimized default rendering settings on first boot so split-screen map views and scrolling render accurately without glitches.

### 2. Controls & Button Mapping
* **Fixed Default Button Mapping (`BASwap=0`)**: **A / B** act as standard buttons and **X / Y** act as autofire buttons out of the box. Setting `BASwap=1` in `nesDS.ini` switches to the diagonal layout (**B / A** normal, **Y / X** autofire).

### 3. Search & Save Path Priority (`nesDS.ini` & `disksys.rom`)
* **App-Directory First**: `nesDS.ini` is searched and newly created in the **same directory as `nesDS.nds`** first, keeping the SD card root clean.
* **`disksys.rom` Search Order**:
  1. ROM directory specified by `StartIn` in `nesDS.ini`
  2. Directory containing `nesDS.nds`
  3. SD card root (`/disksys.rom`)
* **Empty `StartIn` Directory Fix**: Opening an empty `StartIn` folder (`<..>` only) no longer bounces back to the parent directory.

### 4. FDS (Famicom Disk System) Safety Guards
* **BIOS Validation**: Blocks `.fds` loading and shows an error message if `disksys.rom` is missing or not exactly 8,192 bytes.
* **`Save FDS` Lock on Cartridge Games**: Prevents accidental `.fds` overwrites while running cartridge ROMs.

### 5. Enhanced Cheat System
* **Per-Cheat ON/OFF Toggle**: Touching a cheat row's text toggles that cheat between ON (white) and OFF (dark gray). Newly added cheats default to OFF for safety.
* **Human-Readable Text `.cht` Format**: Saves and loads cheats in a readable text format (`ADDR:VAL:TYPE:STATE`), while retaining backward compatibility with legacy 1024-byte binary `.cht` files.
* **Bug Fixes**: Fixed index shifting when deleting a cheat from the middle of the list, and ensured active cheats are cleared when switching ROMs.

### 6. Display, Transition & Audio Stability
* **Clean First-Boot Defaults**: Properly initializes `Blend: None` and `Render Type: SP-Perframe` even without an existing `nesDS.ini`.
* **Smooth ROM Loading Transition**: Blanks screen brightness during `FILE > Load ROM` to eliminate visual artifacts.
* **Audio Sync During File I/O**: Cleanly pauses and resumes ARM7 audio during save state, cheat, and `.ini` file operations to prevent crackling or dropouts.

---

## Installation & Setup

1. Download the latest `nesDS.nds` from the **[Releases](../../releases)** page.
2. Place `nesDS.nds` anywhere on your SD card (e.g., `/_nds/TWiLightMenu/emulators/nesDS.nds` for TWiLight Menu++, or `/roms/nesDS.nds`).
3. *(Optional for FDS)* Place a valid 8,192-byte FDS BIOS named `disksys.rom` in the same folder as `nesDS.nds`, your `StartIn` ROM folder, or the SD card root (`/disksys.rom`).
4. Launch `nesDS.nds` via TWiLight Menu++, flashcart kernel, or HBMenu.

---

## 主な変更点・改善点 (日本語)

### 1. Mapper 95 対応（『ドラゴンバスター』等）
* **Mapper 95（NAMCOT-3425 / MMC3 派生）を新規実装**: CHR バンク切り替えおよび CHR アドレス（A15）に連動したネームテーブルミラーリング制御を実装し、『ドラゴンバスター』などの Mapper 95 ソフトが正常に動作します。
* **描画設定との相性改善**: 初回起動時のデフォルト描画設定を最適化し、ダンジョン地図画面などの分割描画・スクロールが崩れず正常に表示されます。

### 2. 操作性・ボタン配置の改善
* **デフォルトのボタン割り当てを修正 (`BASwap=0`)**: 初期状態で **A / B ボタンが通常の単発・長押し入力**、**X / Y ボタンが連射入力** になります。`nesDS.ini` で `BASwap=1` に設定すると、斜め押し配置（B / A が通常、Y / X が連射）に切り替わります。

### 3. 設定ファイル (`nesDS.ini`) と FDS BIOS (`disksys.rom`) の検索・保存優先順位
* **アプリディレクトリ優先の検索・保存**: `nesDS.ini` を SD カードのルート直下ではなく、**`nesDS.nds` と同じフォルダ** から最優先で読み込み・新規作成します。
* **`disksys.rom` の検索順序**:
  1. `nesDS.ini` の `StartIn` で指定された ROM フォルダ
  2. `nesDS.nds` と同じフォルダ
  3. ルート直下 (`/disksys.rom`)
* **空の `StartIn` ディレクトリ維持**: ROM が入っていない空フォルダ（`<..>` のみ）を `StartIn` に指定した場合に親ディレクトリへ戻ってしまう不具合を修正しました。

### 4. FDS（ディスクシステム）の安全性強化
* **BIOS 未ロード時の起動ブロック**: `disksys.rom` が見つからない、またはサイズが不正（8,192バイト以外）の場合は `.fds` の起動をブロックし、エラーメッセージを表示します。
* **カートリッジ動作中の誤保存防止**: カートリッジ（`.nes` 等）のプレイ中に `Save FDS` が誤動作してファイルを破損する不具合を防止しました。

### 5. チート機能の強化とバグ修正
* **チートコードの個別 ON / OFF 切り替え**: チート一覧画面でコード文字列をタッチすると、個別に ON（白文字）と OFF（ダークグレー文字）を切り替えられます。新規追加時は無効状態（OFF）で追加されます。
* **テキスト形式の `.cht` 対応**: テキストエディタ等で編集しやすい形式（`ADDR:VAL:TYPE:STATE`）での保存・読み込みに対応しました（従来の1024バイトバイナリ `.cht` の読み込み互換性も維持）。
* **バグ修正**: リスト途中削除時のインデックスずれ修正、および別 ROM 読み込み時のチート自動クリアに対応しました。

### 6. 描画・画面遷移・音声の安定化
* **初回起動時の描画設定初期化**: `nesDS.ini` が存在しない初回起動時でも `Blend: None` や `Render Type: SP-Perframe` が正しく適用されます。
* **ROM 切り替え時の画面遷移改善**: `FILE > Load ROM` 実行時に画面の輝度遮光を行い、切り替え時の残像や乱れを抑えました。
* **ファイルアクセス時の音割れ防止**: ステート・チート・`.ini` の保存／読み込み時に ARM7 音声を一時停止・同期再開することでノイズや音割れを防止しました。

---

## 導入方法

1. **[Releases](../../releases)** ページから最新の `nesDS.nds` をダウンロードします。
2. SD カードの任意のフォルダに配置します（例：TWiLight Menu++ で使用する場合は `/_nds/TWiLightMenu/emulators/nesDS.nds`、単体起動の場合は `/roms/nesDS.nds` など）。
3. *(FDS を遊ぶ場合)* 正規の 8,192バイトの FDS BIOS を `disksys.rom` にリネームし、`nesDS.nds` と同じフォルダ、`StartIn` フォルダ、または SD カードのルート直下 (`/disksys.rom`) に配置します。
4. TWiLight Menu++ や各種マジコンのカーネル等から `nesDS.nds` を起動します。

---

## Building from Source

Requires **devkitPro** (`devkitARM` + `libnds`).

```bash
make clean
make
```

## Credits & License
* Based on **NesDS** by **DS-Homebrew** and original authors (huiminghao, Loopy, FluBBa, etc.).
* Distributed under the original project license.
