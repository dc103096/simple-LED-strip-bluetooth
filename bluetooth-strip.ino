#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <FastLED.h>
#include <BluetoothSerial.h> 
#include <Arduino.h>

BluetoothSerial SerialBT; 

#define LED_PIN     23
#define NUM_LEDS    600

int LED_num = NUM_LEDS;

int strip = 5;  //單一燈數

CRGBArray<NUM_LEDS> leds;
CRGBArray<NUM_LEDS> previous_leds;//延遲用

int countOccurrences(String str, char targetChar) {
  int count = 0;
  int index = 0;

  while (true) {
    index = str.indexOf(targetChar, index);
    if (index != -1) {
      count++;
      index++; // 从下一个位置开始搜索
    } else {
      break;
    }
  }

  return count;
}

String splitStringAtSecondIndex(String originalStr, char targetChar) {
  String targetStr = String(targetChar); //將目標字符轉換為字符串
  int count = 0;
  int secondIndex = -1;
  String cutOffPart;
  String endstr = originalStr; //保存原始字符串

  int index = originalStr.indexOf(targetStr); //查找第一次目標字符串出現的位置

  if (index != -1) {
    count++;

    int startIndex = index + targetStr.length(); //第一次出現後的索引位置

    //從第一次出現後的位置開始查找第二次目標字符串出現的位置
    int secondIndex = originalStr.indexOf(targetStr, startIndex);

    if (secondIndex != -1) {
      cutOffPart = originalStr.substring(secondIndex); //提取第二次出現及之後的部分
      endstr.remove(secondIndex); //刪除第二次出現及之後的部分
    }
  }

  return endstr;
}

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  //FastLED.addLeds<WS2812B, LED_PIN, GRB>(previous_leds, NUM_LEDS); //前燈光 測試
  SerialBT.begin("huanna");
  //SerialBT.begin("room light");
  FastLED.setBrightness(26); //預設亮度
  randomSeed(analogRead(A0)); // 使用類比讀取的數值作為亂數種子
}

/*-------------
//藍芽定義

主要藍芽接收格式為:

[X, x, 0, 0, 0]

第一位為大寫，定義模式或設定，如C=Color, A=Animated
第二位為小寫，定義子模式，設模式為C，則子模式如r=RGB, h=HSV
三~五位則為色彩定義或單數值傳輸
---------------*/
const int maxSize = 5; //藍芽數組數量
int let = 0; //計算數據量
int reg[maxSize] = {0};
String data = "";  //接收端數據
String value = ""; //原始數據
String dislocation = ""; //錯位數據
char reg_0;
char reg_1;
String setting = ""; //設定

/*-------------*/
//緩衝與延遲

//時鐘延遲
const unsigned long clock_delayTime = 1000;
unsigned long clock_previous_delayTime = 0;

//燈條延遲
unsigned long strip_delayTime = 500;
unsigned long strip_previous_delayTime = 0;

int special_delayTime[] = {};
int animated_delayTime[] = {};
//unsigned long special_delayTime = 500;
unsigned long special_previous_delayTime = 0;

bool animated_current = false;

bool special_current = false; //緩衝器確認
bool special_step_run = false; //步驟確定完成

//unsigned long animated_delayTime = 0;
unsigned long animated_previous_delayTime = 0;

/*
//燈條緩衝
unsigned long strip_bufferTime = 0;
unsigned long strip_previous_bufferTime = 0;
*/

/*-------------*/
//定義模式
String display_mode = ""; //模式
String display_side = ""; //子模式
String color = "RGB"; //預設顏色

String special_mode = "Constant";//預設效果

String speed_mode = "";
int speed_animated = 1;
int speed_effect = 10;


bool set_light = false;
bool set_speed = true;

int animated_step = 0;//預設
int animated_run = 0;

int special_step = 0;//預設
int strip_buffer = 0;//預設
int special_run = 0;
/*-------------*/
//定義初始狀態
int R = 255;
int G = 255;
int B = 255;

int H = 360;
int S = 100;
int V = 100;

static uint8_t rainbow_hue = 0;
int hue_value = 1;
int currentBrightness = 26;//預設亮度

/*int original_currentBrightness = currentBrightness;
int brightness_intensity = 0;*/

/*特效部分*/
int set_R = 0;
int set_G = 0;
int set_B = 0;

int meteor_size;
int meteorPosition_start;
int meteorBrightness;
/*-------------*/

void loop() {
  unsigned long currentTime = millis();

  if(SerialBT.available()) {
    while ((SerialBT.available())) {
      data = char(SerialBT.read());
      value += data;
    }

    if (dislocation != "") {
      value = dislocation + value;
      dislocation = "";
    }

    let = countOccurrences(value, ',');

    if (!isupper(value.charAt(0))) {
      value = "";
    } else {
      if ((let != maxSize)||(countOccurrences(value, value.charAt(0)) == -1)) {
        if (let > maxSize) {
          value = splitStringAtSecondIndex(value, value.charAt(0));
        } else {
          dislocation = value;
        }
      }
    }

    if ((value!="")&&(dislocation=="")) {
      int value_let = 0;
      String value_data = "";

      for (int i = 0; i < value.length(); i++) {
        char data = value.charAt(i);
        if (data == ',') {
          if (isAlpha(value_data.charAt(0))) {
            reg[value_let] = static_cast<int>(char(value_data.charAt(0)));
          } else {
            reg[value_let] = value_data.toInt();
          }
          value_data = "";
          value_let++;
        } else {
          value_data += data;
        }
      }
      value_let = 0;
    }

    value = "";
    let = 0;
  }

/*-------------*/
  if (char(reg[0]) == 'C') {
    display_mode = "Color";

    if (char(reg[1]) == 'r') { //有數值
      display_side = "RGB";
      R = reg[2];//一定要在這定義
      G = reg[3];
      B = reg[4];
      
    } else if (char(reg[1]) == 'h') { //有數值
      display_side = "HSV";
      H = reg[2];
      S = reg[3];
      V = reg[4];

    }
  } else if (char(reg[0]) == 'A') { //沒有數值
    display_mode = "Animated";
    if (char(reg[1]) == 'r') {
      display_side = "Rainbow";
    } else if (char(reg[1]) == 's') {
      display_side = "Star";
    } else if (char(reg[1]) == 'm') {
      display_side = "Meteor";
    }
  } else if (char(reg[0]) == 'S') { //沒有數值
    if (char(reg[1]) == 'c') {
      special_mode = "Constant";

    } else if (char(reg[1]) == 'f') {
      special_mode = "Flash";

    } else if (char(reg[1]) == 'b') {
      special_mode = "Breath";

    } else if (char(reg[1]) == 'r') {
      special_mode = "Running";

    }
  } else if (char(reg[0]) == 'F') {//有數值
    if (char(reg[1]) == 'l') {
      set_light = true;
      currentBrightness = reg[2];

    } else if (char(reg[1]) == 's') {
      if (char(reg[2]) == 'a') {
        speed_animated = reg[3];

      } else if (char(reg[2]) == 's') {
        speed_effect = reg[3];

      }
    }
  }
/*-------------*/
  if (set_light == true) {
    FastLED.setBrightness(currentBrightness);
    set_light = false;
  }

/*-------------*/



/*-------------*/
  if (display_mode == "Color") {
    if (display_side = "RGB") {
      leds(0, LED_num) = CRGB(R, G, B);
    } else if (display_side = "HSV") {
      leds(0, LED_num) = CHSV(H, S, V);
    }
  } else if (display_mode == "Animated") {
    if (display_side == "Rainbow") {//timer
      animated_delayTime[0] = 0;
    } else if (display_side == "Star") {
      animated_delayTime[0] = 1000 - (speed_animated * 10);
    } else if (display_side == "Meteor") {
      animated_delayTime[0] = 0;
    }

    if ((currentTime - animated_previous_delayTime >= animated_delayTime[animated_step])&&(display_mode == "Animated")) {//動畫緩衝器
      animated_current = true;
      animated_previous_delayTime = currentTime;
    }

    if (display_side == "Rainbow") {
      switch (animated_step) {
        case 0:
          rainbow_hue += round(speed_animated / 2);//無須延遲
          leds.fill_rainbow(rainbow_hue);
          break;
      }
    } else if (display_side == "Star") {
      switch (animated_step) {
        case 0:
          if (animated_current == true) { //緩衝器確認
            animated_current = false;

            for (int i = 0; i < LED_num; i++) {
              if (random(10) < 2) { // 20% 的機率點亮像素點
                leds[i] = CRGB(255, 255, 255); // 使用白色作為星星的顏色
              } else {
                leds[i] = CRGB(0, 0, 0);
              }
              previous_leds[i] = leds[i];
            }
          } else {
            for (int i = 0; i < LED_num; i++) {
             leds[i] = previous_leds[i];
            }
          }
          break;
      }
    } else if (display_side == "Meteor") {
      switch (animated_step) {
        case 0:
          if (animated_run == 0) {
            meteor_size = round(NUM_LEDS / 20);
            meteorPosition_start = random(NUM_LEDS - meteor_size);
            meteorBrightness = random(150, 255);
          }
          
          if (animated_run < meteor_size) {
            animated_run+=2;
            leds(meteorPosition_start, meteorPosition_start + animated_run) = CRGB(meteorBrightness, meteorBrightness, meteorBrightness);
          } else {
            leds(0, LED_num) = CRGB(0, 0, 0);
            animated_run = 0;
          }
          break;
      }
    }
  }

  if (special_mode != "Constant") {
    if (special_mode == "Breath") {//timer
      special_delayTime[0] = 50;
      special_delayTime[1] = 1000;
      special_delayTime[2] = 50;
      special_delayTime[3] = 1000;
    }

    if (currentTime - special_previous_delayTime >= special_delayTime[special_step]) {
      special_current = true;
      special_previous_delayTime = currentTime;
    }

    if (special_mode == "Breath") {
      switch (special_step) {//timer
        case 0:
          for (int i = 0; i < LED_num; i += 1) {
            float reductionFactor = 1.0 / 50.0 * special_run;// 1/10
            set_R = round(leds[i].r - (leds[i].r * reductionFactor));
            set_G = round(leds[i].g - (leds[i].g * reductionFactor));
            set_B = round(leds[i].b - (leds[i].b * reductionFactor));
            leds[i] = CRGB(set_R, set_G, set_B);
          }

          if (special_current == true) { //緩衝器確認
            special_current = false;

            if (special_run == 50) { // 1/10
              special_run = 0;
              special_step++;
            } else {
              special_run++;
            }
          }
          break;
        case 1:
          leds(0, LED_num) = CRGB(0, 0, 0);
          if (special_current == true) { //緩衝器確認
            special_current = false;
            special_step++;
          }
          break;
        case 2:
          for (int i = 0; i < LED_num; i += 1) {
            float increaseFactor = (1.0 / 50.0) * special_run;
            set_R = round(leds[i].r * increaseFactor);
            set_G = round(leds[i].g * increaseFactor);
            set_B = round(leds[i].b * increaseFactor);
            leds[i] = CRGB(set_R, set_G, set_B);
          }

          if (special_current == true) { //緩衝器確認
            special_current = false;

            if (special_run == 50) { // 1/10
              special_run = 0;
              special_step++;
            } else {
              special_run++;
            }
          }
          break;
        case 3:
          if (special_current == true) { //緩衝器確認
            special_current = false;
            special_step = 0;
          }
          break;
      }
    }
  }

  FastLED.show();
}
