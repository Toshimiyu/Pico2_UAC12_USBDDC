# USB CDC診断ログ

[English](CDC_LOG_EN.md) | [日本語](CDC_LOG.md)

## CDC診断ログ

診断ログ出力用の仮想COMポートは、CMakeの`USB_BRINGUP_LEVEL`で選択します。`3`ではCDCインターフェースと診断ログ出力を有効にし、`0`、`1`、`2`ではCDCクラス、インターフェース、エンドポイントを無効にします。そのため、`0`、`1`、`2`では仮想COMポートは認識されず、CDC通信も発生しません。レベル`3`でPCから本機の仮想COMポートを開くと、診断ログが出力されます。

起動レベルは`0`（USB無効）、`1`（TinyUSB初期化のみ・CDCなし）、`2`（通常動作・CDCなし）、`3`（通常動作・CDC診断ログあり）です。既定値は`2`です。診断ログが必要な場合だけ、`-DUSB_BRINGUP_LEVEL=3`を指定してください。既存のCMakeキャッシュに`3`が保存されている場合は、`-DUSB_BRINGUP_LEVEL=2`を指定するか、CMakeキャッシュを削除して再構成してください。

レベル`3`では起動画面の後に診断画面を表示し、再生画面とFFT処理を停止します。`FFT_STATIC_BYTES`およびFFT処理時間の統計は0になります。音声再生および再生開始ログ画面は引き続き利用できます。

CDC出力を有効にしてPCで本機の仮想COMポートを開くと、CDC接続からの経過時間をミリ秒で記録したCSVログを出力します。メタデータにはファームウェア版、プリフィル値、UACモードのほか、`#MEM`行で`.data`開始から`.bss`終端までの静的RAMサイズ、リング割り当てサイズ、FFT静的データサイズ、LCD作業バッファーサイズを記録します。`FFT_STATIC_BYTES`と`LCD_BUFFER_BYTES`は`SRAM_DATA_BSS_BYTES`に含まれる内訳であり、加算する値ではありません。LCD値は描画キュー／作業バッファーを対象とし、Flash上のフォントは含みません。リングはヒープに確保するため別途記録します。ヒープ空き容量とスタック使用量は計測しません。

画面には幅に合わせた短縮版名（既定構成では`2.2-offset`、CDC診断版では`2.2-offset-diag`）を表示します。完全な版名はCDCログの`FW_VERSION`で確認できます。

`#FB_CONFIG`行には、フィードバックの目標水位、基本Kp・Ki、過渡期のゲイン開始倍率と期間、ブースト開始・最大時の偏差、基本・最大補正上限、および固定フィードバック試験の有無を記録します。KpはµHz／フレーム、KiはµHz／（フレーム・秒）です。過渡期はリセット後のフィードバック更新で経過し、Kp・Kiを開始倍率から1倍まで徐々に戻します。ブーストは時間ではなく、目標水位から平滑化水位を引いた正の偏差に応じて変化します。`FB_LIMIT_PPM_X10`はさらにHz上限を適用した実際の上限です。

フィードバック改善の4つの`FB_EXPERIMENTAL_*`オプションと`USB_AUDIO_ENABLE_OFFSET_SEED`は新規構成ではすべて既定で`ON`です。旧CMakeキャッシュは自動更新されないため、5つすべてを明示的に`ON`にするか、新規ディレクトリで構成してください。旧`FB_EXPERIMENTAL_OFFSET_SEED`の値は新オプションが未設定の場合のみ引き継がれます。`-DUSB_AUDIO_ENABLE_OFFSET_SEED=OFF`でオフセット引き継ぎだけを無効にして比較できます。前段の比較オプションも残しており、依存するオプションを有効にしたまま前段を無効にすることはできません。全オプションを`OFF`にすると従来の目標3072フレーム、`FW_VERSION=2.2`に戻ります。既定の構成はリング容量の50%（2048フレーム）、`FW_VERSION=2.2-tgt2048-seed-aw-rate-offset`です。CDC診断版のバージョンには`-diag`が付きます。目標を下げるとアンダーラン側の余裕が減るため、各レートの水位・飽和・無音投入を実機で確認してください。

`FB_EXPERIMENTAL_LEVEL_SEED`は、STREAMING開始後にCore1が新たなPCMでDMAプールを満たしたことを確認し、次のフィードバック更新で平滑化水位を実リング水位に一度だけ合わせます。確認前は従来の平滑化・PI制御が動作し、DMAプールが満杯にならない場合は初期化を待ちます。積分器の停止やリセットは追加しません。

`FB_EXPERIMENTAL_ANTI_WINDUP`は条件付き積分を使用します。P項とホスト差項を含む出力が上限または下限に達し、今回の積分増分がさらに同じ方向へ向かう場合は、その増分を適用しません。逆方向への積分は許可します。

`FB_EXPERIMENTAL_RATE_GAINS`では、Kpは公称レートの0.47ppm／フレーム、Kiは基本Kpの2乗の1/4となります。レート変更に追従し、既存の64msローパス済み水位をそのまま使用します。起動時は両ゲインを同じ倍率でランプするため、その間は実効Kiが実効Kpの2乗の1/4とは限りません。`#FB_CONFIG`のKp・Kiは`GAIN_REFERENCE_HZ`における基本値をµHz単位に丸めたもので、実際の制御値は丸めません。`GAIN_MODE=RATE_SCALED`、`KP_NPPM_PER_FRAME=470`、`KI_FORMULA=KP_SQUARED_DIV4`から他のレートの基本値を求められます。低レートを含め、飽和・水位・整定には実機検証が必要です。

`USB_AUDIO_ENABLE_OFFSET_SEED`では、STREAMING開始後5秒を除き、連続30秒の制限後補正平均から、平滑化水位の前後15秒平均の差を使った傾きをppm換算して引き、次のストリームのI項初期値を推定します。最初の有効値はそのまま保存し、以後は保存値70%・新値30%で更新します。保存値から15ppm以上離れた値は、連続2窓が互いに5ppm以内の場合だけ採用します。±80ppmを超える推定値は保存値も無効にします。ブースト、窓内の50%超の出力飽和、ストリーム中断、レート切替、異常状態またはUSBサスペンドでは窓を破棄します。フィードバック更新の間隔が20msを超えた場合も窓を破棄し、5秒の待機から再開します。USBリセット・切断、ストリーム終了から60分以上の経過、電源断では保存値も無効になります。保存はRAM内のみです。レート切替では保存したppmを新レートのHzに換算します。推定精度と整定改善はホストのフィードバック追従や実機条件に依存し、保証されません。

`OFFSET_SEED,UPTIME_US,RATE,VALID,SAVED_PPM,STOP_AGE_MS`はSTREAMING開始時の保存値・有効性と前回停止からの経過時間（停止履歴がなければ空欄）です。`OFFSET_UPDATE,UPTIME_US,RATE,ESTIMATE_PPM,CORRECTION_PPM,SLOPE_FRAMES_S,SATURATION_PCT,REASON,SAVED_VALID,SAVED_PPM`は窓の結果です。`REASON`は`INITIAL`、`SMOOTHED`、`BOOST`、`SATURATION`、`RANGE`、`PENDING`、`GAP`です。`GAP`では未完成窓の推定値などは空欄です。イベント欠落は`OFFSET_DROPPED,COUNT`で通知し、未接続時のイベントは保存しません。

### 高頻度記録と時刻同期

CDC有効ビルドでは既存の100ms周期行に加え、約1ms間隔で次の行を出力します。周期は保証されません。

`FAST,UPTIME_US,UNIX_US,STATE,RING,FB_LEVEL_X10,FEEDBACK_Q16,FB_INTEG_MILLIHZ,USB_OUT_AGE_US,MISSED,DROPPED,FB_DIAG_MODE,FB_DIAG_AGE_US,FB_P_MILLIHZ,FB_HF_MILLIHZ,FB_CORR_PPM_X10,FB_LIMIT_PPM_X10,FB_OUT_SAT,FB_I_SAT`

- `UPTIME_US`は採取時の起動後µs、`UNIX_US`は同期済みUTCのUnix時刻（µs）です。未同期時は空欄です。
- `STATE`は0=IDLE、1=BUFFERING、2=STREAMING、3=OVERFLOW_FATAL、4=UNDERFLOW_FATAL、5=SWITCHINGです。`RING`はリングのステレオフレーム数、`FB_LEVEL_X10`は平滑化水位の10倍です。異なる値は厳密な同時測定ではありません。
- `FEEDBACK_Q16`は要求レートの16.16 samples/ms表現です。Hzに変換するには`FEEDBACK_Q16 * 1000 / 65536`を計算します。IDLEとSWITCHINGでは0です。`FB_INTEG_MILLIHZ`は積分値の1000倍、`USB_OUT_AGE_US`は最終受信からの経過時間で、取得不可時は空欄です。
- `MISSED`は周期に間に合わず採取できなかった回数、`DROPPED`は出力FIFOの空き不足などで出力できなかった行数の累積です。どちらも接続セッションごとにリセットされ、次に出力できた`FAST`行で確認できます。切断時に未送信の行は失われます。
- 追加列は直近のフィードバック制御更新の記録です。`FB_DIAG_MODE`は`PI`、`BUFFERING`、固定フィードバック試験時の`FIXED`で、更新がまだない場合や状態切り替え直後は全追加列が空欄です。`FB_DIAG_AGE_US`はその更新からFAST採取までの経過時間です。リング水位などとは厳密な同時測定ではありません。
- `FB_P_MILLIHZ`は水位偏差に対するP項、`FB_HF_MILLIHZ`はホストレート差の補正項です。I項は既存の`FB_INTEG_MILLIHZ`を参照してください。`FB_CORR_PPM_X10`は制限後の補正量を公称サンプルレートに対するppmの10倍で示します。基準は公称レートではなく実際のI2Sレートで、`FEEDBACK_Q16`にはさらに量子化があります。`FB_LIMIT_PPM_X10`は実際の出力補正上限のppmの10倍です。整数への変換は0方向に切り捨てます。
- `FB_OUT_SAT`と`FB_I_SAT`はそれぞれ出力と積分値が上限に到達・超過した場合`1`、下限の場合`-1`、それ以外は`0`です。`BUFFERING`と`FIXED`ではP項、ホスト差項、PIの上限・飽和フラグは空欄です。出力飽和中も積分が進む場合があるため、両者を区別してください。

Windowsでは、CDC有効ファームウェアを使用し、他のターミナルを閉じた上で次を実行します。既存ファイルは上書きしません。

```powershell
.\tools\capture_cdc.ps1 -Port COM3 -Output .\capture.csv
```

切断後も同じCOM番号で自動的に記録を再開するには、代わりに次を実行します。

```powershell
.\tools\capture_cdc_auto.ps1 -Port COM3 -OutputDir .\captures
```

接続の試行ごとに別のCSVを作成し、既存ファイルは上書きしません。`capture_events.log`にはPCのローカル時刻とタイムゾーン付きで`START`（収集の試行開始）、`END`（終了理由）、`PORT_BACK`（ポートの不在を検出後に再検出した時刻と待機秒数）を追記します。待機秒数は直前の収集終了から、または初回はラッパー起動からの経過です。エラー後にポートが既に存在する場合は、不在を観測できなかったことを`PORT_PRESENT`に記録します。終了後1秒待ってからポートを確認し、検出後さらに約0.5秒で再試行します。エラーは画面とログに表示します。COM番号が変わった場合は元の番号を待ち続けるため、Ctrl+Cで停止し、新しい番号で再実行してください。ポートの列挙は実際の接続・切断時刻や再接続成功を保証しません。停止はCtrl+Cです。

PC側ツールはCOMポートを開いて`TIME,<UTCのUnix時刻をマイクロ秒で表した整数>`と改行を送信し、受信バイトをファイルに記録します。装置はコマンド処理時の起動後時刻を対応付け、`#TIME_SYNC,<UNIX_US>,<UPTIME_US>`で応答します。10秒以内に応答がない場合やコマンドが拒否された場合、ツールはエラーで終了します。停止はCtrl+Cです。USB CDCの115200はポート設定値であり、USB転送速度の指定ではありません。

CSVはCOMポートを開けた後に作成します。ポートを開けずに失敗した試行はイベントログに残りますが、CSVは作成されません。開いた後の時刻同期などで失敗した場合は、不完全なCSVが残ることがあります。

UTCはPCが送信直前に取得した値を装置のコマンド処理時刻に対応付けるため、PC・USB・装置側の遅延分の誤差があります。PC側の時計精度にも依存し、ハードウェア同期ではありません。長時間の周波数ずれやPCの時計変更を自動補正しません。既存の`MS`や`TRACE`などの時刻列は互換性のため変更せず、`#TIME_SYNC`と`#TRACE_SESSION_START_US`を用いて同じ起動セッション内の時刻を対応付けられます。

高頻度出力はCPU・USB帯域・PCの書き込み負荷を増やし、再生に影響したりログが欠落したりする可能性があります。`MISSED`、`DROPPED`、既存の欠落通知とループ時間を確認し、CDC無効時とも比較してください。この方法でスピンロック保持時間は直接測定できません。

通常行は100ms間隔で、`MS,FREQ,BIT_DEPTH,USB_MODE,HOST_RATE,FEEDBACK_RATE,HF_DIFF_NOW,RING_NOW,RING_MIN,RING_AVG,RING_MAX,MAX_HF_DIFF_SESSION,MAX_HF_DIFF_STREAM,STATE,UNDERFLOW_COUNT,USB_GAP_MAX_US,USBBUF_CAPACITY,USBBUF_FREE_NOW,USBBUF_FREE_MIN_STREAM,USBBUF_FREE_MAX_STREAM,USBBUF_FREE_MIN_PCT_STREAM,LCD_MAX_US,LCD_AVG_US,LCD_COUNT,FFT_MAX_US,FFT_AVG_US,FFT_COUNT,RATE_CHANGE_COUNT,STREAM_START_COUNT,STREAM_STOP_COUNT,LOOP0_LT1MS,LOOP0_LT2MS,LOOP0_LT5MS,LOOP0_GE5MS,LOOP1_LT1MS,LOOP1_LT2MS,LOOP1_LT5MS,LOOP1_GE5MS`の順です。`USB_MODE`はビルド構成に応じた`UAC1`または`UAC2`です。`HOST_RATE`は受信フレーム数から求める250ms窓の推定値です。`HF_DIFF_NOW`は両レートが有効な場合の符号付き`HOST_RATE - FEEDBACK_RATE`で、どちらかが0（未計測または無効）の場合は空欄です。`MAX_HF_DIFF_SESSION`はCDCログセッション中の最大絶対差、`MAX_HF_DIFF_STREAM`は現在のSTREAMING期間中の最大絶対差です。後者はSTREAMING開始後1秒間に重なる推定窓を除外し、有効な測定値が得られるまでは0です。`RING_MIN`/`RING_MAX`は各ログ間隔で観測した最小・最大水位、`RING_AVG`は約1ms間隔でサンプリングした平均水位です。状態遷移は`STATE,MS,FROM,TO,RATE,RING,DMA_POOL,USB_MODE`、異常は`ANOMALY,MS,CODE,DETAIL,USB_MODE,STATE,RATE,RING,DMA_POOL`形式で記録し、遷移・異常時の状態と水位も残します。状態遷移イベントは状態を更新したCore0で記録するため、周期サンプルより高い時間分解能で遷移時刻を比較できます。通常行には日時を付加せず、時刻同期情報から対応付けます。CDC接続中は、診断画面を開いていなくても周期的な診断統計を収集します。

通常行の末尾にはさらに`FB_LEVEL,FB_INTEG_HZ,USB_OUT_AGE_US,USB_RESET_COUNT,USB_SUSPEND_COUNT,USB_RESUME_COUNT,DTR_ASSERT_COUNT,DTR_DEASSERT_COUNT`が続きます。最初の3列はフィードバック用の平滑化リング水位、積分値（Hz）、最後のUSB OUT受信コールからの経過時間（µs）です。受信時刻が未取得、またはSTREAMING以外では経過時間は空欄です。`USB_GAP_MAX_US`は過去の最大受信間隔であり、進行中の受信停止の長さではありません。CSV解析ツールはヘッダーに合わせてください。

`UNDERFLOW_COUNT`と追加した5カウンターは起動後の累積で、COM再接続や診断画面のリセットでは消えません。古いファームウェアの`UNDERFLOW_COUNT`はCOM接続後の差分です。`USB_RESET_COUNT`はTinyUSBのバスリセット完了通知、`USB_SUSPEND_COUNT`と`USB_RESUME_COUNT`はTinyUSBの各コールバックの回数です。DTRカウンターはCDCのライン状態要求で観測したDTRの変化を数えます。バスリセット完了や切断の通知時は内部のDTR状態を未設定に戻しますが、明示的なDTR解除としては数えません。未通知の物理切断や暗黙の状態変化は数えられず、各カウンターは32ビットで周回し、電源断・再起動で失われます。追加イベントやカウンターの収集も診断負荷となります。

UNDERFLOW検出時には、状態変更前の値を`UNDERFLOW,MS,RING,DMA_POOL,RING_POOL,HOST_RATE,FEEDBACK_RATE,FB_LEVEL,FB_INTEG_HZ,USB_GAP_MAX_US,USB_OUT_AGE_US`行として記録します。DMA水位は再生中ブロックを含む上限概算で、各水位は同時点の厳密な測定ではありません。イベントキューが満杯の場合は`EVENT_QUEUE_DROPPED`で欠落を通知します。

USBイベントは`USB_EVENT,UPTIME_US32,GENERATION,OVERWRITTEN_BEFORE,EVENT`として出力します。COM接続時は保持中の直近256件を古い順に再送し、その後のイベントも出力します。容量を超えた場合は古いイベントから上書きされます。`UPTIME_US32`は起動後µsの下位32ビットで約71.6分ごとに周回し、`GENERATION`は診断リセットで変化します。`OVERWRITTEN_BEFORE`は今回の行より前に未送信のまま上書きされた件数です。CDC切断時の送信FIFO内の未送信データは失われます。USBイベントは全パケットの記録ではありません。

`P0000+`はCDC接続判定が偽になったことを示し、DTR解除だけでなくUSB未構成やサスペンドでも発生します。新しい`L`イベントは明示的なライン状態要求のDTR・RTS、`Q`はサスペンド、`V`は復帰を示します。バスリセット完了は`B02000+`です。イベントがないことだけではUSB通信が継続していたとは判断できません。

`USBBUF_CAPACITY`と`USBBUF_FREE_*`は名前にかかわらず音声リングバッファーの容量と空きであり、TinyUSBの受信FIFO容量・空きではありません。

CDC送信FIFOは32 KiBで、PC側の一時的な読み取り停滞中もログを保持します。停滞が長引いてFIFOが不足すると周期行は破棄され、送信再開後に`LOG_ROWS_DROPPED`で件数を報告します。次の周期行のリング・LCD・FFT統計は、最後に出力できた周期行からの累積です。CDC切断時は送信FIFO内の未送信データを破棄します。このFIFOはCDC有効ビルドでのみRAMを使用し、音声バッファーとは別です。

CDC有効ビルドでは、COMポートを開く前から状態遷移とDMA無音補完をRAMに記録し、接続後に`TRACE,UPTIME_US,EVENT,FROM,TO,RATE,RING,DMA_POOL,SILENCE_FRAMES`行として出力します。`STATE`の水位は状態変更直前の値で、DMA水位は再生中のブロックを含む上限概算です。`SILENCE_START`は無音DMAの開始、`SILENCE_RESUME`はPCM DMAへの復帰、`SILENCE_STOP`は復帰前のDMA停止を示します。終了行の`SILENCE_FRAMES`はDMAへ投入した無音フレーム数で、停止時に中断された分も含むため実際の出力数とは限りません。無音が継続中は終了行がまだ出ません。時刻は起動からのマイクロ秒で、`#TRACE_SESSION_START_US`を引くとCDCセッション開始からの時間と比較できます。送信待ちキューは直近128件で、古い記録の上書きは`TRACE_DROPPED`で通知します。送信FIFOへ移った記録は送信待ちキューから除かれ、CDC切断時の未送信データおよび電源断・再起動時の履歴は失われます。画面用の履歴は送信待ちキューとは別に保持します。無音補完の有無だけで全てのプツ音の原因を特定することはできません。

`PCM_START`はPCMブロックのDMA投入開始、`PCM_END`は最後のPCM DMA完了後に次のPCMがない場合、`PCM_ABORT`はPCM DMAの中断・消去を示します。`RATE`はイベント記録時のサンプルレートです。連続するPCMブロックの間には出力せず、停止中の繰り返しのクリアでも重複しません。これらと`SILENCE_*`の時刻はDMA操作付近のソフトウェア時刻であり、PIO FIFOやDACなどの遅延があるため、実際のアナログ出力の開始・終了時刻ではありません。追加記録はDMA処理中にも行われ、実行時間に影響する可能性があります。

録音との比較では、同一起動中の`#TIME_SYNC,<同期UNIX_US>,<同期UPTIME_US>`から、各`TRACE`のUTC時刻を `同期UNIX_US + TRACEのUPTIME_US - 同期UPTIME_US` で求めます。同期前に発生したイベントも、同じ起動中なら符号付きの差で換算できます。録音側の開始時刻と録音機器・経路の遅延も確認し、連続試験信号の波形と照合してください。PCと装置の同期誤差、時計のずれ、録音開始時刻の精度、およびPIO・DAC・録音側の遅延により、両者が完全に一致するとは限りません。

通常再生の開始・再バッファリング時は、サンプルレートに応じた約6ms分と3072フレームの大きい方をリングに蓄積してから出力します。例えば96kHzでは3072フレーム（約32ms）、192kHzでは3072フレーム（約16ms）です。蓄積中は新たなリングデータをDMAへ渡しませんが、既にDMAへ渡したデータは排出します。CSVメタデータの`PREFILL_MS`は時間側の基準、`PREFILL_MIN_FRAMES`は最低フレーム数です。既存のCMake設定`USB_AUDIO_PREFILL_FRAMES`（CSVの`PREFILL_FRAMES`）は、バッファリング以外の状態で残データの出力を開始するための従来のしきい値であり、通常再生のプリフィル量ではありません。

現在のリング容量は4096ステレオフレーム、既定のフィードバック目標は50%の2048フレームです。通常のフィードバック補正上限は100ppmで、平滑化した水位が目標より128フレーム以上低くなると上限を段階的に広げ、512フレーム以上低い場合は1000ppmにします。バッファリング中は基準出力レートに公称サンプルレートの500ppm分を加算して要求します。これらは設定値であり、ホストの追従を保証するものではありません。

異常コードと音声状態履歴の詳細は[診断イベント一覧](DIAGNOSTICS.md)を参照してください。

##### 実機ログ確認（V2.2開発中）

CDC有効ビルド（`USB_BRINGUP_LEVEL=3`）で取得したログを確認しました。以下は個別の短時間試験の結果であり、すべてのホストや再生条件での動作を保証するものではありません。修正後の試験に使用したホストOSと再生ソフトは未確認です。

- UAC1：96 kHz／24-bitと48 kHz／24-bitを含む約2分40秒のCDCセッションで、初回`MS=0`を確認。記録された`UNDERFLOW_COUNT`は0で、`FIFO_TRIM`やDMA無音補完の行はありませんでした。
- UAC2：192 kHz／16-bitと176.4 kHz／16-bitを含む約2分のCDCセッションでも、初回`MS=0`を確認。記録された`UNDERFLOW_COUNT`は0で、`FIFO_TRIM`やDMA無音補完の行はありませんでした。
- UAC2の別試験では、COM接続前の141件を初期イベント`J0000 +`から保持し、`OVERWRITTEN_BEFORE=0`で出力しました。別の上限超過試験では256件が出力され、244件の上書きが報告されました。

これらのログではUNDERFLOWが発生していないため、UNDERFLOW詳細行の実機出力は未検証です。UAC1での64件超のイベント保持、およびAndroidでのCDC同時取得・長時間試験も未確認です。ログに異常が記録されないことは、プツ音がないことや長時間の安定性を保証しません。
