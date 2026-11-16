# USB CDC診断ログ

[English](CDC_LOG_EN.md) | [日本語](CDC_LOG.md)

## CDC診断ログ

診断ログ出力用の仮想COMポートは、CMakeの`USB_BRINGUP_LEVEL`で選択します。`3`ではCDCインターフェースと診断ログ出力を有効にし、`0`、`1`、`2`ではCDCクラス、インターフェース、エンドポイントを無効にします。そのため、`0`、`1`、`2`では仮想COMポートは認識されず、CDC通信も発生しません。レベル`3`でPCから本機の仮想COMポートを開くと、診断ログが出力されます。

起動レベルは`0`（USB無効）、`1`（TinyUSB初期化のみ・CDCなし）、`2`（通常動作・CDCなし）、`3`（通常動作・CDC診断ログあり）です。既定値は`2`です。診断ログが必要な場合だけ、`-DUSB_BRINGUP_LEVEL=3`を指定してください。既存のCMakeキャッシュに`3`が保存されている場合は、`-DUSB_BRINGUP_LEVEL=2`を指定するか、CMakeキャッシュを削除して再構成してください。

CDC出力を有効にしてPCで本機の仮想COMポートを開くと、CDC接続からの経過時間をミリ秒で記録したCSVログを出力します。メタデータにはファームウェア版、プリフィル値、UACモードのほか、`#MEM`行で`.data`開始から`.bss`終端までの静的RAMサイズ、リング割り当てサイズ、FFT静的データサイズ、LCD作業バッファーサイズを記録します。`FFT_STATIC_BYTES`と`LCD_BUFFER_BYTES`は`SRAM_DATA_BSS_BYTES`に含まれる内訳であり、加算する値ではありません。LCD値は描画キュー／作業バッファーを対象とし、Flash上のフォントは含みません。リングはヒープに確保するため別途記録します。ヒープ空き容量とスタック使用量は計測しません。

通常行は100ms間隔で、`MS,FREQ,BIT_DEPTH,USB_MODE,HOST_RATE,FEEDBACK_RATE,HF_DIFF_NOW,RING_NOW,RING_MIN,RING_AVG,RING_MAX,MAX_HF_DIFF_SESSION,MAX_HF_DIFF_STREAM,STATE,UNDERFLOW_COUNT,USB_GAP_MAX_US,USBBUF_CAPACITY,USBBUF_FREE_NOW,USBBUF_FREE_MIN_STREAM,USBBUF_FREE_MAX_STREAM,USBBUF_FREE_MIN_PCT_STREAM,LCD_MAX_US,LCD_AVG_US,LCD_COUNT,FFT_MAX_US,FFT_AVG_US,FFT_COUNT,RATE_CHANGE_COUNT,STREAM_START_COUNT,STREAM_STOP_COUNT,LOOP0_LT1MS,LOOP0_LT2MS,LOOP0_LT5MS,LOOP0_GE5MS,LOOP1_LT1MS,LOOP1_LT2MS,LOOP1_LT5MS,LOOP1_GE5MS`の順です。`USB_MODE`はビルド構成に応じた`UAC1`または`UAC2`です。`HOST_RATE`は受信フレーム数から求める250ms窓の推定値です。`HF_DIFF_NOW`は両レートが有効な場合の符号付き`HOST_RATE - FEEDBACK_RATE`で、どちらかが0（未計測または無効）の場合は空欄です。`MAX_HF_DIFF_SESSION`はCDCログセッション中の最大絶対差、`MAX_HF_DIFF_STREAM`は現在のSTREAMING期間中の最大絶対差です。後者はSTREAMING開始後1秒間に重なる推定窓を除外し、有効な測定値が得られるまでは0です。`RING_MIN`/`RING_MAX`は各ログ間隔で観測した最小・最大水位、`RING_AVG`は約1ms間隔でサンプリングした平均水位です。状態遷移は`STATE,MS,FROM,TO,RATE,RING,DMA_POOL,USB_MODE`、異常は`ANOMALY,MS,CODE,DETAIL,USB_MODE,STATE,RATE,RING,DMA_POOL`形式で記録し、遷移・異常時の状態と水位も残します。状態遷移イベントは状態を更新したCore0で記録するため、周期サンプルより高い時間分解能で遷移時刻を比較できます。日時はデバイスで生成しないため、必要な場合はPC側の測定ソフトで付加してください。CDC接続中は、診断画面を開いていなくても周期的な診断統計を収集します。

通常行の末尾にはさらに`FB_LEVEL,FB_INTEG_HZ,USB_OUT_AGE_US`が続きます。順にフィードバック用の平滑化リング水位、積分値（Hz）、最後のUSB OUT受信コールからの経過時間（µs）です。受信時刻が未取得、またはSTREAMING以外では経過時間は空欄です。`USB_GAP_MAX_US`は過去の最大受信間隔であり、進行中の受信停止の長さではありません。列が追加されるためCSV解析ツールはヘッダーに合わせてください。

UNDERFLOW検出時には、状態変更前の値を`UNDERFLOW,MS,RING,DMA_POOL,RING_POOL,HOST_RATE,FEEDBACK_RATE,FB_LEVEL,FB_INTEG_HZ,USB_GAP_MAX_US,USB_OUT_AGE_US`行として記録します。DMA水位は再生中ブロックを含む上限概算で、各水位は同時点の厳密な測定ではありません。イベントキューが満杯の場合は`EVENT_QUEUE_DROPPED`で欠落を通知します。

USBイベントは`USB_EVENT,UPTIME_US32,GENERATION,OVERWRITTEN_BEFORE,EVENT`として出力します。COM接続時は保持中の直近256件を古い順に再送し、その後のイベントも出力します。容量を超えた場合は古いイベントから上書きされます。`UPTIME_US32`は起動後µsの下位32ビットで約71.6分ごとに周回し、`GENERATION`は診断リセットで変化します。`OVERWRITTEN_BEFORE`は今回の行より前に未送信のまま上書きされた件数です。CDC切断時の送信FIFO内の未送信データは失われます。USBイベントは全パケットの記録ではありません。

`USBBUF_CAPACITY`と`USBBUF_FREE_*`は名前にかかわらず音声リングバッファーの容量と空きであり、TinyUSBの受信FIFO容量・空きではありません。

CDC送信FIFOは32 KiBで、PC側の一時的な読み取り停滞中もログを保持します。停滞が長引いてFIFOが不足すると周期行は破棄され、送信再開後に`LOG_ROWS_DROPPED`で件数を報告します。次の周期行のリング・LCD・FFT統計は、最後に出力できた周期行からの累積です。CDC切断時は送信FIFO内の未送信データを破棄します。このFIFOはCDC有効ビルドでのみRAMを使用し、音声バッファーとは別です。

CDC有効ビルドでは、COMポートを開く前から状態遷移とDMA無音補完をRAMに記録し、接続後に`TRACE,UPTIME_US,EVENT,FROM,TO,RATE,RING,DMA_POOL,SILENCE_FRAMES`行として出力します。`STATE`の水位は状態変更直前の値で、DMA水位は再生中のブロックを含む上限概算です。`SILENCE_START`は無音DMAの開始、`SILENCE_RESUME`はPCM DMAへの復帰、`SILENCE_STOP`は復帰前のDMA停止を示します。終了行の`SILENCE_FRAMES`はDMAへ投入した無音フレーム数で、停止時に中断された分も含むため実際の出力数とは限りません。無音が継続中は終了行がまだ出ません。時刻は起動からのマイクロ秒で、`#TRACE_SESSION_START_US`を引くとCDCセッション開始からの時間と比較できます。送信待ちキューは直近128件で、古い記録の上書きは`TRACE_DROPPED`で通知します。送信FIFOへ移った記録は送信待ちキューから除かれ、CDC切断時の未送信データおよび電源断・再起動時の履歴は失われます。画面用の履歴は送信待ちキューとは別に保持します。無音補完の有無だけで全てのプツ音の原因を特定することはできません。

通常再生の開始・再バッファリング時は、サンプルレートに応じた約6ms分と3072フレームの大きい方をリングに蓄積してから出力します。例えば96kHzでは3072フレーム（約32ms）、192kHzでは3072フレーム（約16ms）です。蓄積中は新たなリングデータをDMAへ渡しませんが、既にDMAへ渡したデータは排出します。CSVメタデータの`PREFILL_MS`は時間側の基準、`PREFILL_MIN_FRAMES`は最低フレーム数です。既存のCMake設定`USB_AUDIO_PREFILL_FRAMES`（CSVの`PREFILL_FRAMES`）は、バッファリング以外の状態で残データの出力を開始するための従来のしきい値であり、通常再生のプリフィル量ではありません。

現在のリング容量は4096ステレオフレーム、フィードバック目標は3072フレーム（75%）です。通常のフィードバック補正上限は100ppmで、平滑化した水位が目標より128フレーム以上低くなると上限を段階的に広げ、512フレーム以上低い場合は1000ppmにします。バッファリング中は基準出力レートに公称サンプルレートの500ppm分を加算して要求します。これらは設定値であり、ホストの追従を保証するものではありません。

異常コードと音声状態履歴の詳細は[診断イベント一覧](DIAGNOSTICS.md)を参照してください。

##### 実機ログ確認（V2.1開発中）

CDC有効ビルド（`USB_BRINGUP_LEVEL=3`）で取得したログを確認しました。以下は個別の短時間試験の結果であり、すべてのホストや再生条件での動作を保証するものではありません。修正後の試験に使用したホストOSと再生ソフトは未確認です。

- UAC1：96 kHz／24-bitと48 kHz／24-bitを含む約2分40秒のCDCセッションで、初回`MS=0`を確認。記録された`UNDERFLOW_COUNT`は0で、`FIFO_TRIM`やDMA無音補完の行はありませんでした。
- UAC2：192 kHz／16-bitと176.4 kHz／16-bitを含む約2分のCDCセッションでも、初回`MS=0`を確認。記録された`UNDERFLOW_COUNT`は0で、`FIFO_TRIM`やDMA無音補完の行はありませんでした。
- UAC2の別試験では、COM接続前の141件を初期イベント`J0000 +`から保持し、`OVERWRITTEN_BEFORE=0`で出力しました。別の上限超過試験では256件が出力され、244件の上書きが報告されました。

これらのログではUNDERFLOWが発生していないため、UNDERFLOW詳細行の実機出力は未検証です。UAC1での64件超のイベント保持、およびAndroidでのCDC同時取得・長時間試験も未確認です。ログに異常が記録されないことは、プツ音がないことや長時間の安定性を保証しません。
