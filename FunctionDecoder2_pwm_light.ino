// DCC Function Decoder Rev 2 for DS-DCC decode
// By yaasan
// Based on Nicolas's sketch http://blog.nicolas.cx
// Inspired by Geoff Bunza and his 17 Function DCC Decoder & updated library
//
// pwm light function
//
// O1:F1 でON/OFF
// O2:テールライト F0でON/OFF 後進で点灯 速度に比例して調光
// O3:F3 でON/OFF
// O4:ヘッドライト F0でON/OFF 前進で点灯 速度に比例して調光
//

#include <NmraDcc.h>
#include <avr/eeprom.h>	 //required by notifyCVRead() function if enabled below

//各種設定、宣言

#define DECODER_ADDRESS 3
#define DCC_ACK_PIN 0   // Atiny85 PB0(5pin) if defined enables the ACK pin functionality. Comment out to disable.
//                      // Atiny85 DCCin(7pin)
#define O1 0            // Atiny85 PB0(5pin)
#define O2 1            // Atiny85 PB1(6pin) analogwrite
#define O3 3            // Atint85 PB3(2pin)
#define O4 4            // Atiny85 PB4(3pin) analogwrite

#define MAX_PWMDUTY 255
#define MID_PWMDUTY 10

#define CV_VSTART		2
#define CV_ACCRATIO		3
#define CV_DECCRATIO	4
#define CV_F0_FORWARD 33
#define CV_F0_BACK 34
#define CV_F1 35
#define CV_F2 36
#define CV_F3 37
#define CV_F4 38
#define CV_F5 39
#define CV_F6 40
#define CV_F7 41
#define CV_F8 42
#define CV_F9 43
#define CV_F10 44
#define CV_F11 45
#define CV_F12 46

//使用クラスの宣言
NmraDcc	 Dcc;
DCC_MSG	 Packet;

//Task Schedule
unsigned long gPreviousL5 = 0;

//進行方向
uint8_t gDirection = 128;

//Function State
uint8_t gState_F0 = 0;
uint8_t gState_F1 = 0;
uint8_t gState_F2 = 0;
uint8_t gState_F3 = 0;
uint8_t gState_F4 = 0;

//モータ制御関連の変数
uint32_t gSpeedRef = 1;

//CV related
uint8_t gCV1_SAddr = 3;
uint8_t gCVx_LAddr = 3;

//Internal variables and other.
#if defined(DCC_ACK_PIN)
const int DccAckPin = DCC_ACK_PIN ;
#endif

struct CVPair {
  uint16_t	CV;
  uint8_t	Value;
};
CVPair FactoryDefaultCVs [] = {
  {CV_MULTIFUNCTION_PRIMARY_ADDRESS, DECODER_ADDRESS}, // CV01
  {CV_ACCESSORY_DECODER_ADDRESS_MSB, 0},               // CV09 The LSB is set CV 1 in the libraries .h file, which is the regular address location, so by setting the MSB to 0 we tell the library to use the same address as the primary address. 0 DECODER_ADDRESS
  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB, 0},          // CV17 XX in the XXYY address
  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB, 0},          // CV18 YY in the XXYY address
  {CV_29_CONFIG, 128 },                                // CV29 Make sure this is 0 or else it will be random based on what is in the eeprom which could caue headaches
  //  {CV_VSTART, 16},
  //  {CV_ACCRATIO, 64},
  //  {CV_DECCRATIO, 64},
};

void(* resetFunc) (void) = 0;  //declare reset function at address 0
void LightMes( char );
uint8_t FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs) / sizeof(CVPair);

//uint16_t limitSpeed(uint16_t inSpeed);

void notifyCVResetFactoryDefault()
{
  //When anything is writen to CV8 reset to defaults.

  resetCVToDefault();
  //Serial.println("Resetting...");
  delay(1000);  //typical CV programming sends the same command multiple times - specially since we dont ACK. so ignore them by delaying

  resetFunc();
};

//------------------------------------------------------------------
// CVをデフォルトにリセット
// Serial.println("CVs being reset to factory defaults");
//------------------------------------------------------------------
void resetCVToDefault()
{
  for (int j = 0; j < FactoryDefaultCVIndex; j++ ) {
    Dcc.setCV( FactoryDefaultCVs[j].CV, FactoryDefaultCVs[j].Value);
  }
};



extern void	   notifyCVChange( uint16_t CV, uint8_t Value) {
  //CVが変更されたときのメッセージ
  //Serial.print("CV ");
  //Serial.print(CV);
  //Serial.print(" Changed to ");
  //Serial.println(Value, DEC);
};

void notifyCVAck(void)
{
  //Serial.println("notifyCVAck");
  digitalWrite(O4,HIGH);
 // analogWrite(O1, 0);
//  analogWrite(O4, 64);

  delay( 6 );
  digitalWrite(O4,LOW);
//  analogWrite(O4, 0);
}


void setup()
{
  uint8_t cv_value;

  //シリアル通信開始
  //Serial.begin(115200);

  //D9,D10 PWM キャリア周期:31kHz
//  TCCR1 &= B11110000;
//  TCCR1 |= B00000001;

  //PWMを10bit化
  //TCCR1A |= B00000011;

//    TCCR0A = 2<<COM0A0 | 2<<COM0B0 | 3<<WGM00;
 //   TCCR0B = 0<<WGM02 | 1<<CS00;    
 //   TCCR1 = 0<<PWM1A | 0<<COM1A0 | 1<<CS10;
 //   GTCCR = 1<<PWM1B | 2<<COM1B0;
#if 0
  TCCR0A = 2<<COM0A0 | 2<<COM0B0 | 3<<WGM00; // B1111 0011
  TCCR0B = 0<<WGM02 | 1<<CS00;               // B0000 0001 分周無し
  TCCR1 = 0<<PWM1A | 0<<COM1A0 | 1<<CS10;
  GTCCR = 1<<PWM1B | 2<<COM1B0;  
#endif

// TCCR1 =0x03;最初に0x03が設定されている。
// D9,D10 PWM キャリア周期:31kHz
// TCCR1 &= B11110000;
// TCCR1 |= B00000001;

//TCCR1 = 1<<CTC1 | 1<<PWM1A | 3<<COM1A0 | 1<<CS10;
  TCCR1 = 0<<CTC1 | 0<<PWM1A | 0<<COM1A0 | 1<<CS10;
  
  pinMode(O1, OUTPUT);
  pinMode(O2, OUTPUT);
  pinMode(O3, OUTPUT);
  pinMode(O4, OUTPUT);

  //DCCの応答用負荷ピン
#if defined(DCCACKPIN)
  //Setup ACK Pin
  pinMode(DccAckPin, OUTPUT);
  digitalWrite(DccAckPin, 0);
#endif

#if !defined(DECODER_DONT_DEFAULT_CV_ON_POWERUP)
  if ( Dcc.getCV(CV_MULTIFUNCTION_PRIMARY_ADDRESS) == 0xFF ) {	 //if eeprom has 0xFF then assume it needs to be programmed
    //Serial.println("CV Defaulting due to blank eeprom");
    notifyCVResetFactoryDefault();

  } else {
    //Serial.println("CV Not Defaulting");
  }
#else
  //Serial.println("CV Defaulting Always On Powerup");
  notifyCVResetFactoryDefault();
#endif

  // Setup which External Interrupt, the Pin it's associated with that we're using, disable pullup.
  Dcc.pin(0, 2, 0); // Atiny85 7pin(PB2)をDCC_PULSE端子に設定

  // Call the main DCC Init function to enable the DCC Receiver
  Dcc.init( MAN_ID_DIY, 100,   FLAGS_MY_ADDRESS_ONLY , 0 );

  //Reset task
  gPreviousL5 = millis();

  //Init CVs
  gCV1_SAddr = Dcc.getCV( CV_MULTIFUNCTION_PRIMARY_ADDRESS ) ;

//デバック信号：起動 　
//注意：2016/4/9 このコメントを外し起動時にアドレスを点灯させるようにすると、
//      loopにたどり着く時間が長くなり、CV書き込みが失敗します。
//LightMes(0);
//LightMes(Dcc.getCV(CV_MULTIFUNCTION_PRIMARY_ADDRESS));
}

void loop() {
  // You MUST call the NmraDcc.process() method frequently from the Arduino loop() function for correct library operation
  Dcc.process();

  if ( (millis() - gPreviousL5) >= 100)
  {
    //Headlight control
    HeadLight_Control();

    //Motor drive control
    //Motor_Control();

    //Reset task
    gPreviousL5 = millis();
  }
}

/* HeadLight control Task (10Hz:100ms) */
void HeadLight_Control()
{

  uint16_t aPwmRef = 0;

#if 0
  if ( gState_F0 > 0)
  {

    if ( gState_F2 > 0)
    {
      if ( gSpeedRef > 1)
      {
        aPwmRef = MAX_PWMDUTY;
      }
      else
      {
        aPwmRef = MID_PWMDUTY;
      }
    }
    else if ( gState_F1 > 0)
    {
      aPwmRef = MID_PWMDUTY;
    }
    else
    {
      aPwmRef = MAX_PWMDUTY;
    }
  }
  else
  {
    aPwmRef = 0;
  }
#endif

//---------------------------------------------------------------------
// PWMによるLED調光
//--------------------------------------------------------------------- 
//#if 0
  if(gState_F0 > 0)
  {
    if( gDirection == 0){                // Reverse 後進(DCS50Kで確認)
      // PB1:O2:6pin PWM 出力 , PB4:O4:3pin LOW出力

      //      OCR1B無効   OCR1B未使用　
      GTCCR = 0; 
      digitalWrite(O4, LOW); // PB4:O4:3pin LOW出力

      //               OCR1A有効   high出力          分周無し　
      TCCR1 = 1<<PWM1A | 2<<COM1A0 | 1<<CS10; 

      // analogWrite(F3, gSpeedRef); // analogWriteは使えない 
      OCR1A = gSpeedRef;               // 0〜255 
      
    } else {                             // Forward 前進(DCS50Kで確認)
      // PB1:O2:6pin LOW出力  , PB4:O4:3pin PWM 出力

      //               OCR1A無効   OCR1A未使用　 分周無し　
      TCCR1 = 1<<CS10; 
      digitalWrite(O2, LOW); // PB1:O2:6pin LOW出力
      //                OCR1B有効     high出力　
      GTCCR = 1 << PWM1B | 2 << COM1B0; 
      analogWrite(O4, gSpeedRef); // 0〜255 
      // OCR1B = gSpeedRef;         // この設定もできる 
    }
  }
  
  if(gState_F0 == 0){                    // DCC F0 コマンドの処理
    TCCR1 = 1<<CS10; 
    digitalWrite(O2, LOW);               // 消灯
    digitalWrite(O4, LOW);               // 消灯
  }
//#endif
  
#if 0
  if(gState_F0 == 0){                    // DCC F0 コマンドの処理
    digitalWrite(O2, LOW);               // アナログ出力:ヘッドライト
    digitalWrite(O4, LOW);   
  } else {
    if( gDirection == 0){                // Reverse 後進(DCS50Kで確認)
      digitalWrite(O2, HIGH); 
      digitalWrite(O4, LOW);
    } else {                             // Forward 前進(DCS50Kで確認)
      digitalWrite(O2, LOW);

      if ( gSpeedRef > 1){               // 調光機能
        aPwmRef = MAX_PWMDUTY;           // 速度が1以上だったらMAX
      } else {
        aPwmRef = MID_PWMDUTY;           // 速度が0だったらMID
      }
      analogWrite(O4, aPwmRef);
    }
  }
#endif

  if(gState_F1 == 0){
    digitalWrite(O1, LOW);
  } else {
    digitalWrite(O1, HIGH);
  }

#if 0
  if(gState_F2 == 0){                   // DCS50Kは1shotしか光らない
    digitalWrite(O3, LOW);  
  } else {
    digitalWrite(O3, HIGH);
  }
#endif

  if(gState_F3 == 0){
    digitalWrite(O3, LOW);
  } else {
    digitalWrite(O3, HIGH);
  }
}

//DCC速度信号の受信によるイベント
extern void notifyDccSpeed( uint16_t Addr, uint8_t Speed, uint8_t ForwardDir, uint8_t MaxSpeed )
{
  if ( gDirection != ForwardDir)
  {
    gDirection = ForwardDir;
  }
  gSpeedRef = Speed;
}

//ファンクション信号受信のイベント
extern void notifyDccFunc( uint16_t Addr, FN_GROUP FuncGrp, uint8_t FuncState)
{
  if ( FuncGrp == FN_0_4) //Function Group 1 F0 F4 F3 F2 F1
  {
    if ( gState_F0 != (FuncState & FN_BIT_00))
    {
      //Get Function 0 (FL) state
      gState_F0 = (FuncState & FN_BIT_00);
    }
    if ( gState_F1 != (FuncState & FN_BIT_01))
    {
      //Get Function 1 state
      gState_F1 = (FuncState & FN_BIT_01);
    }
    if ( gState_F2 != (FuncState & FN_BIT_02))
    {
      //Get Function 0 (FL) state
      gState_F2 = (FuncState & FN_BIT_02);
    }
    
    if ( gState_F3 != (FuncState & FN_BIT_03))
    {
      //Get Function 0 (FL) state
      gState_F3 = (FuncState & FN_BIT_03);
    }
    
    if ( gState_F4 != (FuncState & FN_BIT_04))
    {
      //Get Function 4 (FL) state
      gState_F4 = (FuncState & FN_BIT_04);
    }  
  
  }
}

//#if 0
//-----------------------------------------------------
// F4 を使って、メッセージを表示させる。
// ex 5 -> 0101 -> ー・ー・
//-----------------------------------------------------
void LightMes( char sig )
{
  char cnt;
  for( cnt = 0 ; cnt<8 ; cnt++ ){
    if( sig & 0x01 == 0x01){
      digitalWrite(O1, HIGH); // 長光
      delay(1000);
      digitalWrite(O1, LOW);
      delay(200);
    } else {
      digitalWrite(O1, HIGH); // 短光
      delay(200);
      digitalWrite(O1, LOW);            
      delay(200);
    }
    sig = sig >> 1;
  }
      delay(400);
}
//#endif
