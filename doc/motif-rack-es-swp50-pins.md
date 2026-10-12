# SWP50（T8F02TB-0102）のピンの表

MOTIF-RACK ES のサービスマニュアル（14-15 ページ）の「LSI PIN DESCRIPTION」から起こした。
420 ピンの BGA で、DM 基板の IC22（マスター）と IC27（スレーブ）に使われている。
[motif-rack-es.md](motif-rack-es.md) の SWP50 の節とあわせて読む。

* ピン番号・ボールの位置・名前・向きは、PDF の文字から機械で抜き出した（420 本そろうことを確かめた）
* 役目は表の英語を日本語にしたもの。**表で空欄のものは「（空欄…）」と書き、名前からの見立てを添えた**
* 元の表の誤記: 313 番（AF3、MELI3）が「303」と印刷されている。電源の行にも名前と説明が合わない所がある
  （ここでは名前を優先して VDDC = +1.5V、VDDS = +3V、VSS・VSS2 = GND とした）

## 信号の仲間ごと

| 仲間 | 名前 | 本数 | 向き | 役目 | 覚え書き |
|---|---|---|---|---|---|
| CPU のバス | CD0-15 | 16 | I/O | レジスタのデータ（16bit） | CPU の D[15:0] |
| CPU のバス | CA0-15 | 16 | I | レジスタの番地（16 本） | CPU の A[16:1]。語番地 16bit と合う |
| CPU のバス | CSN0-1 | 2 | I | チップセレクト（2 本） |  |
| CPU のバス | WRN | 1 | I | 書き込みのストローブ |  |
| CPU のバス | RDN | 1 | I | 読み出しのストローブ |  |
| CPU のバス | WAIT0 | 1 | O | CPU を待たせる要求 |  |
| CPU のバス | IRQ0 | 1 | O | 割り込みの要求 |  |
| CPU のバス | DREQ0 | 1 | O | （表は空欄。名前からは DMA の要求） |  |
| 波形メモリ | HMA0-30 | 31 | O | 番地（31 本、上位のデータの側） |  |
| 波形メモリ | LMA0-30 | 31 | O | 番地（31 本、下位のデータの側） |  |
| 波形メモリ | HMD0-15 | 16 | I/O | データ（上位 16bit） |  |
| 波形メモリ | LMD0-15 | 16 | I/O | データ（下位 16bit） |  |
| 波形メモリ | MOEN | 1 | O | 出力の許可 |  |
| 波形メモリ | MWEN | 1 | O | 書き込みの許可 |  |
| DIMM・SDRAM（サンプル用の RAM） | DMAL0-14・DMAH0-14 | 30 | O | 番地（H・L 各 15 本） | RACK ES では使わない側 |
| DIMM・SDRAM（サンプル用の RAM） | DQML0-3・DQMH0-3 | 8 | O | マスク（H・L 各 4 本） |  |
| DIMM・SDRAM（サンプル用の RAM） | DCSL0-3・DCSH0-3 | 8 | O | （空欄。チップセレクト、H・L 各 4 本） |  |
| DIMM・SDRAM（サンプル用の RAM） | DRAS0-1・DCAS0-1・DWEN0-1 | 6 | O | （空欄。RAS・CAS・WE、各 2 本） |  |
| DIMM・SDRAM（サンプル用の RAM） | DCLKIN・DCLK0-3・DCLKE | 6 | I・O | （空欄。クロック 4 本・クロックの許可・クロックの入力） |  |
| エフェクト用の DRAM | HRD0-15 | 16 | I/O | データ（16bit） |  |
| エフェクト用の DRAM | LRD0-15 | 16 | I/O | データ（下位の 16bit） | 回路図ではスレーブ（IC27）の LRD は全部 NC |
| エフェクト用の DRAM | RA0-13 | 14 | O | 番地（14 本） |  |
| エフェクト用の DRAM | RWEN | 1 | O | 書き込みの許可 |  |
| エフェクト用の DRAM | RCAS | 1 | O | CAS |  |
| エフェクト用の DRAM | RRAS | 1 | O | RAS |  |
| エフェクト用の DRAM | RQML・RCLK・RCLKE・RCLKIN・RQMH | 5 | I・O | （空欄。マスク 2 本・クロック・クロックの許可・クロックの入力） |  |
| チップ間・ボードとの音の線（MEL） | MELO0-7 | 8 | O | 音のデータの出力（8 本） |  |
| チップ間・ボードとの音の線（MEL） | MELI0-7 | 8 | I | 音のデータの入力（8 本） |  |
| 音の出口のクロック | WCLK0-1 | 2 | O | DAC の LR クロック（2 本） |  |
| 音の出口のクロック | CK128 | 1 | O | マスタークロック（256 Fs） | 名前は CK128、表の説明は 256 Fs |
| 音の出口のクロック | BCLK | 1 | O | マスタークロック（64 Fs） |  |
| 音の出口のクロック | CK512 | 1 | O | （空欄。名前からは 512 Fs） |  |
| 同期 | SY0 | 1 | O | 同期のクロック（出力） |  |
| 同期 | SY1 | 1 | I | 同期のクロック（入力） |  |
| 同期 | KONTRG0-1 | 2 | I・O | キーオンのデータ（0 = 出力、1 = 入力） | マスターとスレーブで発音の時刻を合わせる線らしい |
| 同期 | SLAVE | 1 | I | マスター / スレーブの選択 |  |
| ABUS | ADAT0-15 | 16 | I/O | データ（16bit） |  |
| ABUS | AFRM・ACLK・ADIR | 3 | I/O・O | （空欄。フレーム・クロック・向き） |  |
| デジタル出力ほか | ADLR・DITO | 2 | O | （空欄。DITO は名前からはデジタル出力の送り） |  |
| E-Bus | EIRQ・EICN・ESDA・ESCL | 4 | I/O・O | （空欄。割り込み・初期化・データ・クロック） | 起動の流れ図の「E-Bus」。PLG ボードとの線らしい |
| クロックと PLL | XO・XI | 2 | I・O | 水晶（XI = 入力、XO = 出力） |  |
| クロックと PLL | RFCLK0-1 | 2 | I・O | PLL のクロック（0 = 出力、1 = 入力） |  |
| クロックと PLL | PLL_AVD・PLL_AVS | 2 | ― | PLL の電源 |  |
| クロックと PLL | PLL_TSTN・PLL_BP | 2 | I | （表は「Power supply」と空欄。名前からは PLL の試験・バイパス） |  |
| リセットと試験 | ICN | 1 | I | 初期化（イニシャルクリア） |  |
| リセットと試験 | TCK・TRST・TMS・TDO・TDI | 5 | I・O | JTAG（表では「Test pin」。TDI は空欄） | スレーブはテストランド TL6-TL10 に出ている |
| リセットと試験 | TMODE・TEST1 | 2 | I | 試験用 |  |
| そのほか | NC | 2 | ― | 未使用 |  |
| 電源 | VDDC | 17 | ― | +1.5V（コア） |  |
| 電源 | VDDS | 35 | ― | +3V（入出力） |  |
| 電源 | VSS・VSS2 | 52 | ― | GND |  |

本数の合計は 420。

## SWP30 と比べて目につく所

* 波形メモリの番地が **H 側と L 側で別々に 31 本ずつ**出ている。データも H・L の 16bit ずつで、合わせて 32bit 幅
* サンプル用の RAM（DIMM・SDRAM）の口が、波形メモリとは別に H・L の 2 組ある。鍵盤版のサンプリング用で、RACK ES では要らない側
* エフェクト用の DRAM の口（HRD・LRD・RA）が独立している。回路図ではスレーブの LRD が全部 NC なので、
  **マスターもスレーブも、エフェクト用メモリは 16bit 幅で使っている**
* レジスタの番地は 16 本（CA0-CA15）、データは 16bit。Firm の読みで出した「語番地 16bit」と合う
* チップ間の音の線は MELI・MELO が 8 本ずつ。同期用に SY0・SY1 と KONTRG0・KONTRG1 がある

## RACK ES でのつなぎ方（DM 基板の回路図、PDF の 104・105 ページ）

マスター（IC22）の周りを回路図で読んだ。スレーブ（IC27）は波形メモリの側と JTAG だけ見比べた。

### 波形 ROM（2 枚めの図）

| SWP50 の端子 | つなぎ先 |
|---|---|
| LMA0-LMA22 | `S50_LMA[0..22]` → L 側の ROM 3 個（IC36-38）の A0-A22 |
| LMA23-LMA27 | NC |
| **LMA28・LMA29・LMA30** | **`SW50_LCS[2]`・`[1]`・`[0]`** = L 側の ROM のチップセレクト（IC36 の /CE が `SW50_LCS[0]`） |
| LMD0-LMD15 | 33Ω を通って `WAV_LMD[0..15]` → ROM の Q0-Q15 |
| HMA0-HMA22、HMA28-30、HMD0-15 | 同じ形で H 側の ROM 3 個（IC39-41）へ。チップセレクトは `SW50_HCS[2..0]` |
| MOEN | 68Ω を通って ROM の /OE |
| MWEN | NC（書き込みは無い） |

* **番地の上位 3 本が、そのまま ROM 3 個のチップセレクトになっている。** 1 個は 23 本ぶん = 8M 語 = 128Mbit
* ROM は 48 ピンで BHE を 3.3V に固定（16bit 幅）。回路図の部品名は「WAVE-ROM L 64/128M 共用」
* **マスターとスレーブは同じ線（`S50_LMA`・`S50_HMA`・`SW50_LCS`・`SW50_HCS`）につながっている。**
  波形 ROM は 2 個の SWP50 の共用で、番地のバスを 2 個で分け合う
* Firm の検査の表（0x01000000 語ごとに 3 区画、偶数番地と奇数番地で別のチップ）と合う。
  語番地の bit 0 が L / H、その上の 23bit が ROM の番地、区画の番号がチップセレクト、という読みになる
* 同じ図に DIMM のソケット 2 個（CN19・CN20）の大きな枠があり、全体が Not installed

### CPU の側（マスター）

| SWP50 の端子 | つなぎ先 |
|---|---|
| CA0-CA15 | `SA[1..16]`（CPU の番地を緩衝した線） |
| CD0-CD15 | `SD[0..15]` |
| CSN0 | `/MSWPCS`（スレーブは `/SSWPCS`） |
| CSN1 | 22k でプルアップ（R150 で `/16-CS3` へつなぐ場所がある） |
| WRN / RDN | `/SWE1/DQMLU` / `/SRD` |
| WAIT0 | L5 を通って `/WAIT` |
| IRQ0 | 33Ω を通って `IRQ3SWP`（CPU の IRQ3 らしい） |
| DREQ0 | 33Ω とプルアップ。`IRQ0SIO` の線のそばに描かれている（行き先は読み切れていない） |
| KONTRG0 / KONTRG1 | マスターとスレーブの間（22k つき） |
| TDI・TCK・TRST・TDO・TMS | テストランド TL1-TL5（スレーブは TL6-TL10）。10k でプルダウン |

### クロックと同期（マスター）

* **水晶 X4 = 11.2896MHz**（= 256 × 44,100Hz）が XI・XO に付いている
* SLAVE は GND（マスター）。スレーブは 3.3V
* 出していく線: SYO、BCLK（図の名前は `MBCLK`）、CK128（`128FS`）、CK512、RFCLKo（68Ω × 2）。RFCLKi は 0Ω で戻している
* PLL_AVD は +1.5V（`+1.5D_M`）

### エフェクト用のメモリ（マスター）

* **IC26（64Mbit の SDRAM、16bit 幅）が 1 個**。HRD0-15 → `MHRD[0..15]`、RA0-13 → `MRA[0..13]`
* **LRD0-15 は全部 NC**（スレーブと同じ）。RQML も NC で、RQMH が SDRAM の LDQM・UDQM の両方へ行く
* RCLK は 68Ω を通って RCLKIN へ戻し、同じ線が SDRAM のクロック
* つまり **エフェクト用のメモリは、チップ 1 個につき 16bit × 4M 語**

### 音の線ほか（マスター）

* MELO0-7 → 33Ω → `MMELO[0..7]`、MELI0-7 ← `MMELI[0..7]`（22k でプルダウン）
* WCLK0・WCLK1 → `MWCLK0`・`MWCLK1`、ADLR → `MADLR`、DITO → `MDITO`（33Ω ずつ）
* ADAT0-15・ADIR・ACLK・AFRM は `ABUS[0..15]` ほかへ。行き先の 40 ピンのコネクター CN9（A-BUS）は Not installed
* ESDA・ESCL・EICN は E-Bus の線へ（EIRQ は図では線が出ていない）

### 音の出口と MEL の線（2026-10-12、読めた所まで）

**この PDF の回路図は、図の左側（A-E 列）がページの枠の外へはみ出している。** 枠の外の部分は
`inkscape --export-area-drawing` で起こせるが、線と文字が欠けた切れ端しか残っていない。
下は、枠の中（F-Q 列）とブロック図で確かめられた所と、切れ端から拾えた所を分けて書く。

確かめられた所:

* **DAC への道**: マスターの SWP50 → IC16（74LVC245、DM 基板）→ CN21（12 ピン）→ JK 基板の CN108 → IC133（バッファ）→
  AK4393 が 3 個（IC112 = OUTPUT L/R、IC113 = ASSIGNABLE 1/2、IC114 = ASSIGNABLE 3/4）
* IC16 を通る線（B → A の向き、直列の抵抗つき）: `MADLR`・`MWCLK0`・`MWCLK1`・`256FS`（= MRFCLKo）・`CK512`・`128FS`・`MBCLK`・`SYO`
* JK 基板の側の名前: `DAC-MCLK`・`DAC-BICK`・`DAC-LRCK`・`DAC-SDATA0`・`DAC-SDATA1`・`DAC-SDATA2`・`/DAC-PD`・`MUTE`。
  DAC 1 個に SDATA が 1 本ずつ。**3 本の SDATA が DM 基板のどの線か（MMELO の何番か）は、切れている側なので決められていない**
* **デジタル出力**: `MDITO`（マスターの DITO）が IC133 を通り、光の出口（JK103）と、IC106（ラインドライバ）+ パルストランスを通って
  同軸の出口（JK102）へ。スレーブの DITO（`SDITO`）も線は出ているが、行き先は切れている
* ヘッドホンは OUTPUT L/R と同じ DAC（IC112）から、ボリュームと HPVR 基板のアンプを通る
* **ゲートアレイ IC9（SIO4）は音の線とは関係が無い**。直列の送受信（RX0-2・TX0-2・TX30-33、8MHz の発振子つき）で、
  MIDI と PLG ボードとのやりとり用。CPU のバスの 8bit の側（`/8-CS`）にぶら下がっている
* JK 基板には **E-BUS CPU（IC129、8.38MHz）** がいて、パネルのスイッチ・LED・エンコーダーを受け持つ。DM 基板とは E-Bus（CN7 ↔ CN115）でつながる。
  SWP50 の ESDA・ESCL・EICN はこの E-Bus の線

切れ端から拾えた所（確かさは低い）:

* PLG のコネクターのそばに 5V ↔ 3V の変換（IC4、74LVC245 ほか）があり、**PLG から来る側に `MMELI[0]`・`MMELI[1]`**、
  **PLG へ行く側に `128FS`・`MWCLK1`・`MMELO[4]`** の名前が残っている。
  「PLG ボードの音はマスターの MELI0・MELI1 へ入り、ボードへはクロックと MMELO4 が行く」という読みになる
* `SMELI[2]`、`SMELO[0…` の名前も近くに残っているが、つながりは読めない
* コネクターは PLG1・PLG2 と、点線の枠（Not installed）の PLG3 がある

**下の「鍵盤版の回路図で補った所」で大半が埋まった。** このとき分からなかったこと: マスターとスレーブの間で MEL の線がどうつながるか（スレーブの音がマスターの MELI へ入るのか、別々に DAC へ行くのか）。
鍵盤版（MOTIF ES6/7/8）のサービスマニュアルの同じ所を見るのが次の手

### 鍵盤版の回路図で補った所（2026-10-12）

鍵盤版（MOTIF ES6/7/8）のサービスマニュアルは図が欠けていない（PDF の 170 ページ = ブロック図、171・172 = DM、173 = DM_SUB と JK）。
DM 基板は IC の番号も線の名前も RACK 版と同じなので、RACK 版で切れていた左側をこれで補った。
**鍵盤版の図で読んだもので、RACK 版の実物で確かめたものではない。**

* **CN21（12 ピン、RACK 版で DAC の基板へ行くコネクター）は、鍵盤版では Not installed で、CN6 の線の分かれ**。
  1 = CN6_27、2 = CN6_28、3 = CN6_24、4 = CN6_18、5 = CN6_14、6 = CN6_11、7 = CN6_2、8 = CN6_5、9 = CN6_4、10 = CN6_3、11・12 = GND
* CN6 の 3-10 番が `MMELO[0..7]`（0Ω つき）、11 番が `/DACIC`。相手の DM_SUB 側（番号は 31 − n）の名前から、
  CN6_14 = WCLK0、CN6_18 = RFCLK、CN6_24 = BCLK（64Fs）
* したがって CN21 は **8 = MMELO[2]、9 = MMELO[1]、10 = MMELO[0]、5 = WCLK0（LR クロック）、4 = RFCLK（256Fs = DAC のマスタークロック）、
  3 = BCLK、6 = /DACIC**。**DAC 3 個のデータは MMELO0・1・2** ということになる（どれがどの DAC かは RACK 版の JK 側で未確認）
* 鍵盤版の注記では **MMELO[1] が「L/R Output」、MMELO[0] が「INDIV 1・2」**。MMELO[7] も DM_SUB へ行く
* **1 本の MEL の線がステレオ 1 組**（DAC 1 個に SDATA 1 本）
* **PLG ボード**: IC4（74LVC245、5V → 3V）を通って **`MMELI[0]`・`[1]`・`[2]` = ボード 1・2・3 の音**。同じ石で RX0-2（ボードからの直列）。
  IC5（74VHCT245、3V → 5V）を通ってボードへ TX0-2・`128FS`・`MWCLK1`・`MMELO[4]`。
  `SMELI[0..2]` へも分けられる抵抗（RA8）があるが Not installed
* **スレーブの MEL**（`SMELO[0..7]`・`SMELI[0..7]`）とマスターの `MMELI[0..7]` は CN23 から DM_SUB へ出ていく（鍵盤版では mLAN・AIEB2・A/D 用）。
  DM 基板の上で、スレーブの MELO がマスターの MELI へ直接つながる線は見当たらない
* ブロック図では **マスターとスレーブが「A-Bus」で結ばれている**。スレーブの音がマスターへ渡る道は、MEL ではなく
  A-Bus（ADAT0-15・ADIR・ACLK・AFRM）と見ている。Not installed の 40 ピンの CN9 は、その線を外へ出す口
* クロック: マスターが `SYO`・`RFCLKo`・`/ICN` を出し、スレーブが `SYI`・`RFCLKi`・`/ICN` で受ける（ブロック図）
* ブロック図の番地の割り当て: CS0 = フラッシュ（**8M バイト**）、CS3 = SDRAM、CS4 = SUB のフラッシュ（**4M バイト**）、
  CS5 = 8bit の周辺（LCD・SIO4）、CS6 = 16bit の周辺（SWP50 のマスター・スレーブ）。鍵盤版の数字で、RACK 版のブロック図は IC32 を 32Mbit と書いている

### A-Bus（RACK 版の回路図で確かめた、2026-10-12）

* **マスターとスレーブの ADAT0-15・ADIR・ACLK・AFRM は、同じ名前の線（`ABUS[0..15]`・`ADIR`・`ACLK`・`AFRM`）で結ばれている。**
  どちらも端子のすぐ外に 33Ω が入り、スレーブの側で 10k のプルアップ（RA63-RA72）が付く。同じ束が Not installed の CN9 へも行く
* つまり **2 個の SWP50 の間の音の道は A-Bus**（16bit の並列 + 向き・クロック・フレーム）。MEL ではない
* スレーブの上辺のほか: SYI と RFCLKi は入力として線がある。**SYO・BCLK・CK128・CK512・RFCLKo は NC**（クロックを出すのはマスターだけ）。
  SLAVE は 3.3V。ESDA・ESCL・EIRQ・EICN は 10k でプルアップされているだけ（E-Bus につながるのはマスターだけ）
* どちら向きに何が流れるか（スレーブ → マスターの片道か、エフェクトの送り返しがあるか）は回路図からは分からない。ADIR が向きを決める線

## 全ピン

| 番号 | ボール | 名前 | 向き | | 番号 | ボール | 名前 | 向き | | 番号 | ボール | 名前 | 向き |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | E5 | VSS2 | ― | | 141 | J25 | LMA3 | O | | 281 | AF10 | DRAS1 | O |
| 2 | D4 | VDDC | ― | | 142 | L22 | LMA16 | O | | 282 | AE10 | DCSH2 | O |
| 3 | C3 | CD15 | I/O | | 143 | L23 | LMA0 | O | | 283 | AF9 | DQMH2 | O |
| 4 | B2 | CD13 | I/O | | 144 | K24 | LMA2 | O | | 284 | AD10 | DCSH3 | O |
| 5 | A1 | CD14 | I/O | | 145 | J26 | LMA14 | O | | 285 | AC10 | VDDC | ― |
| 6 | D5 | CD6 | I/O | | 146 | K25 | LMA15 | O | | 286 | AB10 | VSS2 | ― |
| 7 | E6 | CD2 | I/O | | 147 | K26 | LMA1 | O | | 287 | AE9 | DQMH0 | O |
| 8 | C4 | CD9 | I/O | | 148 | M22 | VSS | ― | | 288 | AF8 | DWEN1 | O |
| 9 | B3 | CD11 | I/O | | 149 | M23 | VDDS | ― | | 289 | AD9 | DCAS1 | O |
| 10 | A2 | CD12 | I/O | | 150 | L24 | LMA22 | O | | 290 | AE8 | DCLK2 | O |
| 11 | A3 | CD10 | I/O | | 151 | L25 | LMA23 | O | | 291 | AC9 | VDDS | ― |
| 12 | D6 | CD1 | I/O | | 152 | L26 | LMA24 | O | | 292 | AB9 | VDDC | ― |
| 13 | E7 | VSS | ― | | 153 | M24 | LMA27 | O | | 293 | AD8 | DCLK3 | O |
| 14 | C5 | CD5 | I/O | | 154 | M25 | LMA28 | O | | 294 | AF7 | MELO0 | O |
| 15 | B4 | CD8 | I/O | | 155 | N22 | LMA25 | O | | 295 | AE7 | MELO1 | O |
| 16 | A4 | CD7 | I/O | | 156 | N23 | LMA26 | O | | 296 | AD7 | MELO2 | O |
| 17 | D7 | VSS2 | ― | | 157 | N24 | LMA30 | O | | 297 | AC8 | VDDS | ― |
| 18 | C6 | CD0 | I/O | | 158 | M26 | LMA29 | O | | 298 | AB8 | VSS | ― |
| 19 | E8 | VSS | ― | | 159 | N25 | MOEN | O | | 299 | AF6 | MELO3 | O |
| 20 | D8 | VDDS | ― | | 160 | N26 | MWEN | O | | 300 | AE6 | MELO4 | O |
| 21 | B5 | CD4 | I/O | | 161 | P26 | LMD15 | I/O | | 301 | AF5 | MELO5 | O |
| 22 | A5 | CD3 | I/O | | 162 | P22 | VSS | ― | | 302 | AC7 | MELO6 | O |
| 23 | C7 | CA2 | I | | 163 | P23 | VDDS | ― | | 303 | AD6 | MELO7 | O |
| 24 | B6 | CA0 | I | | 164 | P24 | LMD13 | I/O | | 304 | AB7 | WCLK0 | O |
| 25 | E9 | CA8 | I | | 165 | P25 | LMD14 | I/O | | 305 | AC6 | WCLK1 | O |
| 26 | D9 | CA9 | I | | 166 | R25 | LMD11 | I/O | | 306 | AE5 | EIRQ | O |
| 27 | C8 | CA5 | I | | 167 | R24 | LMD10 | I/O | | 307 | AF4 | EICN | O |
| 28 | A6 | CA1 | I | | 168 | R23 | VDDS | ― | | 308 | AD5 | ESDA | I/O |
| 29 | B7 | CA3 | I | | 169 | R22 | VSS | ― | | 309 | AE4 | ESCL | I/O |
| 30 | A7 | CA4 | I | | 170 | R26 | LMD12 | I/O | | 310 | AB6 | MELI0 | I |
| 31 | E10 | VSS2 | ― | | 171 | T26 | LMD9 | I/O | | 311 | AC5 | MELI1 | I |
| 32 | D10 | VDDC | ― | | 172 | T25 | LMD8 | I/O | | 312 | AD4 | MELI2 | I |
| 33 | C9 | CA10 | I | | 173 | T24 | LMD7 | I/O | | 313 | AF3 | MELI3 | I |
| 34 | B8 | CA6 | I | | 174 | T23 | VSS2 | ― | | 314 | AE3 | MELI4 | I |
| 35 | A8 | CA7 | I | | 175 | T22 | VSS | ― | | 315 | AF2 | MELI5 | I |
| 36 | B9 | CA11 | I | | 176 | U26 | LMD6 | I/O | | 316 | AB5 | VSS2 | ― |
| 37 | E11 | CA14 | I | | 177 | U25 | LMD5 | I/O | | 317 | AC4 | VDDC | ― |
| 38 | D11 | CA15 | I | | 178 | V26 | LMD3 | I/O | | 318 | AD3 | MELI6 | I |
| 39 | C10 | CA13 | I | | 179 | U24 | LMD4 | I/O | | 319 | AE2 | MELI7 | I |
| 40 | A9 | CA12 | I | | 180 | U23 | VDDC | ― | | 320 | AF1 | ADLR | O |
| 41 | B10 | CSN0 | I | | 181 | U22 | VSS2 | ― | | 321 | AB4 | DITO | O |
| 42 | A10 | CSN1 | I | | 182 | V25 | LMD2 | I/O | | 322 | AA5 | VSS | ― |
| 43 | E12 | VSS | ― | | 183 | W26 | LMD0 | I/O | | 323 | AC3 | AFRM | I/O |
| 44 | D12 | VDDS | ― | | 184 | V24 | LMD1 | I/O | | 324 | AD2 | ACLK | I/O |
| 45 | C11 | WRN | I | | 185 | W25 | DCSL0 | O | | 325 | AE1 | ADIR | O |
| 46 | B11 | RDN | I | | 186 | V23 | VDDS | ― | | 326 | AD1 | ADAT0 | I/O |
| 47 | A11 | WAIT0 | O | | 187 | V22 | VDDC | ― | | 327 | AA4 | VDDS | ― |
| 48 | C12 | IRQ0 | O | | 188 | W24 | DCSL1 | O | | 328 | Y5 | ADAT9 | I/O |
| 49 | B12 | DREQ0 | O | | 189 | Y26 | DQML3 | O | | 329 | AB3 | ADAT3 | I/O |
| 50 | E13 | TCK | I | | 190 | Y25 | DQML1 | O | | 330 | AC2 | ADAT1 | I/O |
| 51 | D13 | TRST | I | | 191 | Y24 | DMAL14 | O | | 331 | AC1 | ADAT2 | I/O |
| 52 | C13 | VSS | ― | | 192 | W23 | VDDS | ― | | 332 | Y4 | ADAT10 | I/O |
| 53 | A12 | XO | O | | 193 | W22 | VSS | ― | | 333 | AA3 | ADAT6 | I/O |
| 54 | B13 | XI | I | | 194 | AA26 | DMAL13 | O | | 334 | W5 | VSS | ― |
| 55 | A13 | VDDS | ― | | 195 | AA25 | DMAL12 | O | | 335 | W4 | VDDS | ― |
| 56 | A14 | SLAVE | I | | 196 | AB26 | DMAL9 | O | | 336 | AB2 | ADAT4 | I/O |
| 57 | E14 | TMS | I | | 197 | Y23 | VSS | ― | | 337 | AB1 | ADAT5 | I/O |
| 58 | D14 | TDO | O | | 198 | AA24 | DMAL11 | O | | 338 | Y3 | ADAT11 | I/O |
| 59 | C14 | ICN | I | | 199 | Y22 | VSS | ― | | 339 | AA2 | ADAT7 | I/O |
| 60 | B14 | RFCLK0 | O | | 200 | AA23 | DMAL10 | O | | 340 | V5 | ADAT14 | I/O |
| 61 | B15 | PLL_TSTN | I | | 201 | AB25 | DMAL8 | O | | 341 | V4 | ADAT15 | I/O |
| 62 | C15 | PLL_BP | I | | 202 | AC26 | DMAL6 | O | | 342 | W3 | ADAT13 | I/O |
| 63 | D15 | VDDS | ― | | 203 | AB24 | DMAL7 | O | | 343 | AA1 | ADAT8 | I/O |
| 64 | E15 | VSS | ― | | 204 | AC25 | DMAL5 | O | | 344 | Y2 | ADAT12 | I/O |
| 65 | A15 | RFCLK1 | I | | 205 | AA22 | VSS2 | ― | | 345 | Y1 | TDI | I |
| 66 | A16 | VDDC | ― | | 206 | AB23 | VSS | ― | | 346 | U5 | VSS2 | ― |
| 67 | B16 | TMODE | I | | 207 | AC24 | DMAL4 | O | | 347 | U4 | VDDC | ― |
| 68 | C16 | PLL_AVD | ― | | 208 | AD26 | DMAL3 | O | | 348 | V3 | HRD13 | I/O |
| 69 | D16 | NC | ― | | 209 | AD25 | DMAL2 | O | | 349 | W2 | HRD15 | I/O |
| 70 | E16 | NC | ― | | 210 | AE26 | DMAL0 | O | | 350 | W1 | HRD14 | I/O |
| 71 | A17 | PLL_AVS | ― | | 211 | AB22 | VSS2 | ― | | 351 | V2 | HRD12 | I/O |
| 72 | B17 | TEST1 | I | | 212 | AC23 | VDDC | ― | | 352 | T5 | HRD7 | I/O |
| 73 | A18 | VSS | ― | | 213 | AD24 | DMAL1 | O | | 353 | T4 | HRD6 | I/O |
| 74 | C17 | SY1 | I | | 214 | AE25 | DCSL2 | O | | 354 | U3 | HRD10 | I/O |
| 75 | D17 | VDDC | ― | | 215 | AF26 | DRAS0 | O | | 355 | V1 | HRD11 | I/O |
| 76 | E17 | VSS2 | ― | | 216 | AC22 | DCAS0 | O | | 356 | U2 | HRD9 | I/O |
| 77 | B18 | KONTRG0 | O | | 217 | AB21 | VDDS | ― | | 357 | U1 | HRD8 | I/O |
| 78 | A19 | KONTRG1 | I | | 218 | AD23 | DCLKIN | I | | 358 | R5 | VSS | ― |
| 79 | C18 | CK512 | O | | 219 | AE24 | DQML2 | O | | 359 | R4 | VDDS | ― |
| 80 | B19 | CK128 | O | | 220 | AF25 | DCSL3 | O | | 360 | T3 | HRD5 | I/O |
| 81 | D18 | BCLK | O | | 221 | AF24 | DQML0 | O | | 361 | T2 | HRD4 | I/O |
| 82 | E18 | SY0 | O | | 222 | AC21 | VDDS | ― | | 362 | T1 | HRD3 | I/O |
| 83 | C19 | HMA20 | O | | 223 | AB20 | VSS | ― | | 363 | R3 | HRD2 | I/O |
| 84 | A20 | HMA21 | O | | 224 | AD22 | DWEN0 | O | | 364 | R2 | HRD1 | I/O |
| 85 | B20 | HMA19 | O | | 225 | AE23 | DCLK0 | O | | 365 | P5 | VDDS | ― |
| 86 | C20 | HMA18 | O | | 226 | AF23 | DCLK1 | O | | 366 | P4 | HRD0 | I/O |
| 87 | D19 | VDDS | ― | | 227 | AC20 | DCLKE | O | | 367 | P3 | RWEN | O |
| 88 | E19 | VSS | ― | | 228 | AD21 | HMD13 | I/O | | 368 | R1 | RQML | O |
| 89 | A21 | HMA9 | O | | 229 | AB19 | VSS | ― | | 369 | P2 | RCAS | O |
| 90 | B21 | HMA7 | O | | 230 | AC19 | VDDS | ― | | 370 | P1 | RRAS | O |
| 91 | A22 | HMA6 | O | | 231 | AE22 | HMD15 | I/O | | 371 | N1 | RA13 | O |
| 92 | D20 | HMA8 | O | | 232 | AF22 | HMD14 | I/O | | 372 | N5 | VDDS | ― |
| 93 | C21 | HMA10 | O | | 233 | AD20 | HMD10 | I/O | | 373 | N4 | VDDS | ― |
| 94 | E20 | HMA17 | O | | 234 | AE21 | HMD12 | I/O | | 374 | N3 | RA10 | O |
| 95 | D21 | VDDS | ― | | 235 | AB18 | VDDC | ― | | 375 | N2 | RA12 | O |
| 96 | B22 | HMA11 | O | | 236 | AC18 | VDDS | ― | | 376 | M2 | RA1 | O |
| 97 | A23 | HMA4 | O | | 237 | AD19 | HMD7 | I/O | | 377 | M3 | RA2 | O |
| 98 | C22 | HMA5 | O | | 238 | AF21 | HMD11 | I/O | | 378 | M4 | VDDS | ― |
| 99 | B23 | HMA13 | O | | 239 | AE20 | HMD9 | I/O | | 379 | M5 | VSS | ― |
| 100 | E21 | VSS | ― | | 240 | AF20 | HMD8 | I/O | | 380 | M1 | RA0 | O |
| 101 | D22 | HMA12 | O | | 241 | AB17 | VSS2 | ― | | 381 | L1 | RA3 | O |
| 102 | C23 | HMA3 | O | | 242 | AC17 | VDDC | ― | | 382 | L2 | RA4 | O |
| 103 | A24 | HMA14 | O | | 243 | AD18 | HMD4 | I/O | | 383 | L3 | RA5 | O |
| 104 | B24 | HMA2 | O | | 244 | AE19 | HMD6 | I/O | | 384 | L4 | VSS2 | ― |
| 105 | A25 | HMA1 | O | | 245 | AF19 | HMD5 | I/O | | 385 | L5 | VSS | ― |
| 106 | E22 | VSS2 | ― | | 246 | AE18 | HMD3 | I/O | | 386 | K1 | RA6 | O |
| 107 | D23 | VDDC | ― | | 247 | AB16 | VSS | ― | | 387 | K2 | RA7 | O |
| 108 | C24 | HMA15 | O | | 248 | AC16 | VSS2 | ― | | 388 | J1 | RA9 | O |
| 109 | B25 | HMA16 | O | | 249 | AD17 | HMD1 | I/O | | 389 | K3 | RA8 | O |
| 110 | A26 | HMA22 | O | | 250 | AF18 | HMD2 | I/O | | 390 | K4 | VDDC | ― |
| 111 | E23 | HMA25 | O | | 251 | AE17 | HMD0 | I/O | | 391 | K5 | VSS2 | ― |
| 112 | F22 | VDDS | ― | | 252 | AF17 | DCSH0 | O | | 392 | J2 | RA11 | O |
| 113 | D24 | HMA27 | O | | 253 | AB15 | VSS | ― | | 393 | H1 | RCLK | O |
| 114 | C25 | HMA0 | O | | 254 | AC15 | VDDS | ― | | 394 | J3 | RCLKE | O |
| 115 | B26 | HMA23 | O | | 255 | AD16 | DCSH1 | O | | 395 | H2 | RCLKIN | I |
| 116 | C26 | HMA24 | O | | 256 | AE16 | DQMH3 | O | | 396 | J4 | VDDS | ― |
| 117 | F23 | VDDS | ― | | 257 | AF16 | DQMH1 | O | | 397 | J5 | VDDC | ― |
| 118 | G22 | HMA26 | O | | 258 | AD15 | DMAH14 | O | | 398 | H3 | RQMH | O |
| 119 | E24 | HMA30 | O | | 259 | AE15 | DMAH13 | O | | 399 | G1 | LRD15 | I/O |
| 120 | D25 | HMA28 | O | | 260 | AB14 | VSS | ― | | 400 | G2 | LRD14 | I/O |
| 121 | D26 | HMA29 | O | | 261 | AC14 | VSS2 | ― | | 401 | G3 | LRD13 | I/O |
| 122 | G23 | LMA17 | O | | 262 | AD14 | DMAH11 | O | | 402 | H4 | VDDS | ― |
| 123 | F24 | LMA19 | O | | 263 | AF15 | DMAH12 | O | | 403 | H5 | VSS | ― |
| 124 | H22 | VSS | ― | | 264 | AE14 | DMAH10 | O | | 404 | F1 | LRD12 | I/O |
| 125 | H23 | VDDS | ― | | 265 | AF14 | DMAH9 | O | | 405 | F2 | LRD11 | I/O |
| 126 | E25 | LMA20 | O | | 266 | AF13 | DMAH8 | O | | 406 | E1 | LRD8 | I/O |
| 127 | E26 | LMA21 | O | | 267 | AB13 | VDDS | ― | | 407 | G4 | VDDS | ― |
| 128 | G24 | LMA9 | O | | 268 | AC13 | VDDS | ― | | 408 | F3 | LRD10 | I/O |
| 129 | F25 | LMA18 | O | | 269 | AD13 | DMAH6 | O | | 409 | G5 | VDDS | ― |
| 130 | J22 | LMA12 | O | | 270 | AE13 | DMAH7 | O | | 410 | F4 | LRD9 | I/O |
| 131 | J23 | LMA4 | O | | 271 | AE12 | DMAH4 | O | | 411 | E2 | LRD7 | I/O |
| 132 | H24 | LMA6 | O | | 272 | AD12 | DMAH3 | O | | 412 | D1 | LRD5 | I/O |
| 133 | F26 | LMA8 | O | | 273 | AC12 | VDDS | ― | | 413 | E3 | LRD6 | I/O |
| 134 | G25 | LMA7 | O | | 274 | AB12 | VSS | ― | | 414 | D2 | LRD4 | I/O |
| 135 | G26 | LMA10 | O | | 275 | AF12 | DMAH5 | O | | 415 | F5 | VSS2 | ― |
| 136 | K22 | VSS2 | ― | | 276 | AF11 | DMAH2 | O | | 416 | E4 | VSS | ― |
| 137 | K23 | VDDC | ― | | 277 | AE11 | DMAH1 | O | | 417 | D3 | LRD3 | I/O |
| 138 | J24 | LMA13 | O | | 278 | AD11 | DMAH0 | O | | 418 | C1 | LRD2 | I/O |
| 139 | H25 | LMA11 | O | | 279 | AC11 | VSS | ― | | 419 | C2 | LRD1 | I/O |
| 140 | H26 | LMA5 | O | | 280 | AB11 | VSS | ― | | 420 | B1 | LRD0 | I/O |
