# Arduino_SmileFunctionDecoder2 for pwm_light
<br>
SmileFunctionDecoder2 (AYA002-2)用の車両ファンクッションデコーダスケッチです。<br>
速度に比例して、ヘッドライト及びテールサイトを調光します。<br>
<br>
デフォルトのアドレス3<br>
O1:F1 でON/OFF<br>
O2:テールライト F0でON/OFF 後進で点灯 速度に比例して調光<br>
O3:F3 でON/OFF<br>
O4:ヘッドライト F0でON/OFF 前進で点灯 速度に比例して調光<br>
<br>
LightMes()関数搭載(^^;<br>
ATtiny85デコーダはシリアルピンが無いため、Atiny85 PB0(5pin):O1を使って、<br>
変数の値を光で確認できます。<br>
※出力ポートはO1,O2,O3,O4のいづれかに変更できます。<br>
※アドレス書きかえ時に影響あるところに入れるとアドレス変更に失敗します。<br>
<br>
