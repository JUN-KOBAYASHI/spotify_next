/* -----------------------------------------------------------------------------------------
 * M5AtomS3 を使って spotify で再生中のタイトルとカバー写真を表示し画面を押すと次のリストに
 * 進むプログラム
 * 2024/7/4 Create KOBAYASHI Jun.
 * 2024/8/6 Update KOBAYASHI Jun.
 * -----------------------------------------------------------------------------------------
*/ 

#include "M5Unified.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include "spotify_logo.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// utf8対応
#include "efontEnableJa.h"
#include "efont.h"

// sprite
M5Canvas SpriteB;
M5Canvas SpriteC;

// ===================================================================================================
// WiFi
const char* ssid1     = "SSID1";
const char* ssid1_key = "12345678";
const char* ssid2     = "SSID2";
const char* ssid2_key = "12345678";
const char* ssid3     = "SSID3";
const char* ssid3_key = "12345678";

// resize url
const String resize_url = "https://example.com/_resize/_resize.php?_u=";

// spotify
const char* client_id = "********************************";
const char* client_secret = "********************************";
String refresh_token = "***********************************************************************************************************************************";


// ===================================================================================================

String spotifyAccessToken = "";
String check_url = "";
bool is_play = false;
bool is_processing = false;


// 画面の明るさ 、処理バッファ
#define BRIGTNESS 60
#define BACKGROUND_BUFFER_SIZE 120000
unsigned long background_buffer_length;
unsigned char background_buffer[BACKGROUND_BUFFER_SIZE];

// spotify logo
extern const uint8_t spotify_logo[];


// タイマー割り込み
hw_timer_t* timer = NULL;
volatile uint32_t counter = 0;
volatile uint32_t current_time = 0;

// IMU
float accX = 0;
float accY = 0;
float accZ = 0;

float gyroX = 0;
float gyroY = 0;
float gyroZ = 0;

float temp = 0;
int rotate_angle = 0;
#define RAD_TO_DEG 57.324

// WiFi 定義
WiFiMulti WiFiMulti;

//-------------------------------------------------------------------------------------------
// タイマー処理
//-------------------------------------------------------------------------------------------
void onTimer() {
  if (is_processing != true) {
    is_processing = true;
    counter++;
    current_time = millis();
    Serial.printf("No. %u, %u ms\n", counter, current_time);
    String old_url = check_url;
  }
}





//-------------------------------------------------------------------------------------------
// setup
//-------------------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("## setup");
  auto cfg = M5.config();
  cfg.internal_imu = true;
  M5.begin(cfg);
  M5.Display.begin();
  M5.Imu.begin();

  // タイマ作成
  timer = timerBegin(0, 80, true);
  // タイマ割り込みサービス・ルーチン onTimer を登録
  timerAttachInterrupt(timer, &onTimer, true);
  // 割り込みタイミング(ms)の設定 100000 = 0.1s
  timerAlarmWrite(timer, 10000000, true);
  // タイマ有効化
  timerAlarmEnable(timer);


  // WiFi  
  Serial.println("wifi begin");
  WiFiMulti.addAP(ssid1, ssid1_key);
  WiFiMulti.addAP(ssid2, ssid2_key);
  WiFiMulti.addAP(ssid3, ssid3_key);

  bool done = true;
  while (done) {
    Serial.print("WiFi connecting");
    auto last = millis();
    while (WiFiMulti.run() != WL_CONNECTED && last + 5000 > millis()) {
      delay(500);
      Serial.println("retry");
    }
    done = false;
  }

  // WiFi Conneted
  Serial.println("\nWiFi connected.");
  Serial.printf("+ SSID : %s\n", WiFi.SSID());
  Serial.printf("+ IP   : %s\n", WiFi.localIP().toString().c_str());

  // spotify access token refresh
  if (spotifyAccessToken.length() == 0) {
    Serial.println("## call Post_refresh_api");
    Post_refresh_api();
  }

  Serial.println("+ spotify logo");
  update_lotate();
  M5.Display.setBrightness(BRIGTNESS);
  M5.Display.setRotation(rotate_angle);
  SpriteB.createSprite(128, 128);
  SpriteB.fillScreen(TFT_BLACK);
  SpriteB.drawJpg(spotify_logo, sizeof spotify_logo, 0, 0);  //背景の表示
  SpriteB.pushSprite(&M5.Display, 0, 0);
  SpriteB.deleteSprite();
}

//-------------------------------------------------------------------------------------------
// loop
//-------------------------------------------------------------------------------------------
void loop() {
  M5.update();  // 本体状態更新

  // 処理中であれば
  if (is_processing != false) {
    Serial.println("#### is_processing");
    String old_url = check_url;
    bool now_play = Get_is_playing();
    if (old_url != check_url) {
      if (is_play == true) {
        M5.Display.wakeup();
        M5.Display.setBrightness(BRIGTNESS);
        update_lotate();
        if (Get_api_playback() != true) {
          // refresh token を更新したかもしれないので１回だけrerunする
          Get_api_playback();
        };
      }
    }

    // 再生中でなければ画面をオフにする
    if (is_play == false) {
      M5.Display.sleep();
      M5.Display.setBrightness(0);
    }
    is_processing = false;
  }

  //ボタンが押されていれば
  if (M5.BtnA.wasClicked()) { 
    Serial.println("#### BtnA.wasClicked");

      if (Post_next_api() != true) {
        // refresh token を更新したかもしれないので１回だけリランする
        Post_next_api();
      };
      Get_api_playback();
  }

  if (M5.BtnA.wasHold()) {  //ボタンが0.5秒以上押されていれば
    Serial.println("#### BtnA.wasHold");
    update_lotate();

    M5.Display.wakeup();
    M5.Display.setBrightness(BRIGTNESS);

    if (Get_api_playback() != true) {
      // refresh token を更新したかもしれないので１回だけリランする
      Get_api_playback();
    };
  }

  if (M5.BtnA.wasDoubleClicked()) {  // ボタンを0.5秒以内に2回クリックして0.5秒経過したか
    Serial.println("#### BtnA.wasDoubleClicked");
    update_lotate();

    if (timerStarted(timer)) {
      Serial.println("#### STOP timer");
      // Stop and free timer
      timerStop(timer);
    } else {
      Serial.println("#### STart timer");
      // Stop and free timer
      timerStart(timer);
    }


    if (Post_start_stop_api() == true) {
      bool now_play = Get_is_playing();
      Serial.println(now_play);

      if (is_play == true) {
        M5.Display.wakeup();
        M5.Display.setBrightness(BRIGTNESS);
        if (Get_api_playback() != true) {
          // refresh token を更新したかもしれないので１回だけリランする
          Get_api_playback();
        };
      } else {
        Serial.println("+ is_play : false");
      }
    }
  }

}


//-------------------------------------------------------------------------------------------
// Post_start_stop_api - 再生の開始 / 停止
//-------------------------------------------------------------------------------------------
bool Post_start_stop_api() {
  Serial.println("## Post_api(start_Stop)");
  String start_api_url = "https://api.spotify.com/v1/me/player/play";
  String stop_api_url = "https://api.spotify.com/v1/me/player/pause";
  String api_url = "";

  bool now_play = Get_is_playing();
  Serial.println(now_play);

  if (is_play == true) {
    Serial.println("+ is_play: true");
    api_url = stop_api_url;
  } else {
    Serial.println("+ is_play: falase");
    api_url = start_api_url;
  }

  HTTPClient httpClient;

  // debug / token
  // Serial.println(spotifyAccessToken);

  // URL 設定
  httpClient.begin(api_url);
  // Content-Type
  httpClient.addHeader("Content-Type", "application/x-www-form-urlencoded");
  // Authorization
  httpClient.addHeader("Authorization", "Bearer " + spotifyAccessToken);
  // Content-Length
  httpClient.addHeader("Content-Length", "0");
  // PUTする
  int status_code = httpClient.PUT("");
  Serial.println(status_code);

  if (status_code == 200 || status_code == 204) {
    Serial.println("+ status_code: 200 & 204");
    // String response = httpClient.getString();
    // Serial.println(response);
    httpClient.end();

    if (is_play == true) {
      Serial.println("++ is_play: true");
      M5.Display.fillScreen(TFT_BLACK);  //M5Stackの画面初期化
      is_play = false;
      M5.Display.sleep();
      M5.Display.setBrightness(0);
    } else {
      Serial.println("++ is_play: false");
      is_play = true;
    }
    return true;
  }

  if (status_code == 401) {
    Serial.println("+ status_code: 401");
    Post_refresh_api();

    M5.update();  // 本体状態更新
    httpClient.end();
    return false;
  }

  if (status_code == 404) {
    // spotifyが動いていないなど
    Serial.println("+ status_code: 404");

    M5.update();  // 本体状態更新
    httpClient.end();
    return false;
  }

  Serial.print("+ status_code: ");
  Serial.println(status_code);
  httpClient.end();
  return false;
}



//-------------------------------------------------------------------------------------------
// Post_next_api - 現在再生中のリストの次の曲を再生
//-------------------------------------------------------------------------------------------
bool Post_next_api() {
  Serial.println("## Post_api(Next)");
  String api_url = "https://api.spotify.com/v1/me/player/next";

  HTTPClient httpClient;

  // debug / token
  // Serial.println(spotifyAccessToken);

  // URL 設定
  httpClient.begin(api_url);
  // Content-Type
  httpClient.addHeader("Content-Type", "application/x-www-form-urlencoded");
  // Authorization
  httpClient.addHeader("Authorization", "Bearer " + spotifyAccessToken);
  // Content-Length
  httpClient.addHeader("Content-Length", "0");
  // ポストする
  int status_code = httpClient.POST("");
  Serial.println(status_code);

  if (status_code == 200 || status_code == 204) {
    Serial.println("+ status_code: 200 & 204");
    // String response = httpClient.getString();
    // Serial.println(response);
    httpClient.end();
    return true;
  }

  if (status_code == 401) {
    Serial.println("+ status_code: 401");
    Post_refresh_api();

    delay(500);   //500ms待機
    M5.update();  // 本体状態更新
    httpClient.end();
    return false;
  }

  if (status_code == 404) {
    // spotifyが動いていないなど
    Serial.println("+ status_code: 404");

    delay(500);   //500ms待機
    M5.update();  // 本体状態更新
    httpClient.end();
    return false;
  }

  Serial.print("+ status_code: ");
  Serial.println(status_code);
  httpClient.end();
  return false;
}


//-------------------------------------------------------------------------------------------
// Get_api_playback - 現在再生中の曲情報の取得
//-------------------------------------------------------------------------------------------
bool Get_api_playback() {
  Serial.println("## Get_api_playback");
  String api_url = "https://api.spotify.com/v1/me/player";


  HTTPClient httpClient;

  // debug / token
  // Serial.println(spotifyAccessToken);

  // URL 設定
  httpClient.begin(api_url);
  // Content-Type
  httpClient.addHeader("Content-Type", "application/x-www-form-urlencoded");
  // Launguage(日本語を指定)
  httpClient.addHeader("Accept-Language", "ja");
  // Authorization
  httpClient.addHeader("Authorization", "Bearer " + spotifyAccessToken);
  // Content-Length
  httpClient.addHeader("Content-Length", "0");
  // GETする
  int status_code = httpClient.GET();
  Serial.println(status_code);

  // 正常な場合
  if (status_code == 200 || status_code == 204) {
    Serial.println("+ status_code: 200 & 204");
    String response = httpClient.getString();
    // Serial.println(response);
    // Serial.println("--------------");
    httpClient.end();
    StaticJsonDocument<3000> playback;
    DeserializationError error = deserializeJson(playback, response);
    if (error) Serial.println("Deserialization error.");
    else {
      String device_name = playback["device"]["name"];
      String title = playback["item"]["name"];
      String is_playing = playback["is_playing"];
      String img_url = playback["item"]["album"]["images"][0]["url"];
      String song_url = playback["item"]["external_urls"]["spotify"];
      String artist = playback["item"]["artists"][0]["name"];
      //Serial.println(is_playing);
      Serial.println("--------------");
      Serial.print("title            : ");
      Serial.println(title);
      Serial.print("artist           : ");
      Serial.println(artist);
      Serial.print("device           : ");
      Serial.println(device_name);
      Serial.print("img url(300x300) : ");
      Serial.println(img_url);
      Serial.print("song url         : ");
      Serial.println(song_url);
      check_url = song_url;
      background_buffer_length = sizeof(background_buffer);
      String get_url = resize_url + img_url;
      Serial.println(get_url);
      long ret = doHttpGet(get_url, background_buffer, &background_buffer_length);
      if (ret != true) {
        Serial.println("doHttpGet Error");
        return true;
      }
      Serial.println("----> output");
      update_lotate();
      M5.Display.setRotation(rotate_angle);

      //------------------------------------------------------------------------------------
      String title_artist = title + " / " + artist;
      // Serial.println(title_artist);
//      SpriteB.createSprite(128, 128);
      if (!SpriteB.createSprite(128, 128)) {
          Serial.println("### [NG] Failed to create SpriteB. Out of memory?");
      } else {
          Serial.println("### [OK] SpriteB created successfully.");
      }
      SpriteB.setTextScroll(false);
      SpriteB.setTextWrap(false);
      // SpriteB.setFont(&fonts::lgfxJapanGothicP_36); // 文字はきれいだけど表示できないものがある
      SpriteB.setFont(&fonts::efontJA_24);
      SpriteB.setTextSize(1.8);
      SpriteB.fillScreen(TFT_BLACK);
        SpriteB.pushSprite(&M5.Display, 0, 0);

      int text_width = SpriteB.textWidth(title_artist);
      Serial.print("[debug] text width : ");
      Serial.println(text_width);
      if(text_width < 600){
        text_width = 800;
      }
      // 流れる文字処理
      for (int i = 128; i >= ((text_width * 2) * -1) - 128; i--) {
        SpriteB.fillScreen(TFT_BLACK);
        SpriteB.setTextColor(TFT_WHITE);
        SpriteB.setCursor(i, 45);
        SpriteB.print(title_artist);
        SpriteB.pushSprite(&M5.Display, 0, 0);
      }
      Serial.println("---");
      Serial.println(((text_width * 2) * -1) - 128);
      Serial.println("---");
      SpriteB.deleteSprite();
      if (!SpriteC.createSprite(128, 128)) {
          Serial.println("### [NG] Failed to create SpriteC. Out of memory?");
      } else {
          Serial.println("### [OK] SpriteC created successfully.");
      }
      SpriteC.drawJpg(background_buffer, sizeof background_buffer, 0, 0);  // カバー画像表示
      SpriteC.pushSprite(&M5.Display, 0, 0);
      SpriteC.deleteSprite();


    }
    return true;
  }

  if (status_code == 401) {
    Serial.println("+ status_code: 401");
    Post_refresh_api();

    delay(500);   //500ms待機
    M5.update();  // 本体状態更新
    httpClient.end();
    return false;
  }

  if (status_code == 404) {
    // spotifyが動いていないなど
    Serial.println("+ status_code: 404");

    delay(500);   //500ms待機
    M5.update();  // 本体状態更新
    httpClient.end();
    return false;
  }

  Serial.print("+ status_code: ");
  Serial.println(status_code);
  httpClient.end();
  return false;
}


//-------------------------------------------------------------------------------------------
// doHttpGet - httpによるデータ取得処理
//-------------------------------------------------------------------------------------------
long doHttpGet(String url, uint8_t* p_buffer, unsigned long* p_len) {
  Serial.println("## doHttpGet");
  HTTPClient http;

  Serial.print("[HTTP] GET begin...\n");
  // configure traged server and url
  http.begin(url);

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();
  unsigned long index = 0;

  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      // get tcp stream
      WiFiClient* stream = http.getStreamPtr();

      // get lenght of document (is -1 when Server sends no Content-Length header)
      int len = http.getSize();
      Serial.printf("[HTTP] Content-Length=%d\n", len);
      if (len != -1 && len > *p_len) {
        Serial.printf("[HTTP] buffer size over\n");
        http.end();
        return false;
      }

      // read all data from server
      while (http.connected() && (len > 0 || len == -1)) {
        // get available data size
        size_t size = stream->available();

        if (size > 0) {
          // read up to 128 byte
          if ((index + size) > *p_len) {
            Serial.printf("[HTTP] buffer size over\n");
            http.end();
            return false;
          }
          int c = stream->readBytes(&p_buffer[index], size);

          index += c;
          if (len > 0) {
            len -= c;
          }
        }
        delay(1);
      }
    } else {
      http.end();
      return false;
    }
  } else {
    http.end();
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    return false;
  }

  http.end();
  *p_len = index;

  return true;
}


//-------------------------------------------------------------------------------------------
// Post_refresh_api - spotifyのrefresh_token 処理
//-------------------------------------------------------------------------------------------
void Post_refresh_api() {
  Serial.println("## Post_refresh_api");
  String refresh_url = "https://accounts.spotify.com/api/token";
  DynamicJsonDocument doc(2048);

  HTTPClient httpClient;

  String json = "grant_type=refresh_token&refresh_token=" + refresh_token;

  // URL 設定
  httpClient.begin(refresh_url);
  // Content-Type
  httpClient.addHeader("Content-Type", "application/x-www-form-urlencoded");
  // Authorization
  httpClient.setAuthorization(client_id, client_secret);
  // ポストする
  Serial.println("+ json");
  Serial.println(json);
  int status_code = httpClient.POST(json);
  if (status_code == 200) {
    Serial.println("+ Status code 200");
    String response = httpClient.getString();
    //    Serial.println(response);
    deserializeJson(doc, response);
    spotifyAccessToken = (const char*)doc["access_token"];
    //    Serial.println("+ spotifyAccessToken:");
    //    Serial.println(spotifyAccessToken);
  }

  if (status_code == 415) {
    Serial.println("+ Status code 415");
  }
  httpClient.end();
  Serial.println("##--- Post_refresh_api end ---");
}

//-------------------------------------------------------------------------------------------
// Get_is_playing - 再生状況の取得処理
//-------------------------------------------------------------------------------------------
bool Get_is_playing() {
  Serial.println("## Get_is_playing");
  String api_url = "https://api.spotify.com/v1/me/player";

  is_play = false;

  HTTPClient httpClient;

  // URL 設定
  httpClient.begin(api_url);
  // Content-Type
  httpClient.addHeader("Content-Type", "application/x-www-form-urlencoded");
  // Authorization
  httpClient.addHeader("Authorization", "Bearer " + spotifyAccessToken);
  // Content-Length
  httpClient.addHeader("Content-Length", "0");
  // GETする
  int status_code = httpClient.GET();
  // Serial.println(status_code);
  // Serial.println(spotifyAccessToken);


  if (status_code == 200 || status_code == 204) {
    Serial.println("+ status_code: 200 & 204");
    if (status_code == 200) {
      String response = httpClient.getString();
      // Serial.println(response);
      httpClient.end();
      StaticJsonDocument<3000> playback;
      DeserializationError error = deserializeJson(playback, response);
      if (error) Serial.println("Deserialization error.");
      else {
        String is_playing = playback["is_playing"];
        String song_url = playback["item"]["external_urls"]["spotify"];
        Serial.println("--------------");
        Serial.print("song url         : ");
        Serial.println(song_url);
        check_url = song_url;
        Serial.print("is_playing : ");
        Serial.println(is_playing);
        if (is_playing == "true") {
          is_play = true;
        }
      }
      return true;
    } else {
      httpClient.end();
      is_play = false;
      return false;
    }
  }

  if (status_code == 401) {
    Serial.println("+ status_code: 401");
    Post_refresh_api();

    delay(500);   //500ms待機
    M5.update();  // 本体状態更新
    httpClient.end();
    is_play = false;
    return false;
  }

  if (status_code == 404) {
    // spotifyが動いていないなど
    Serial.println("+ status_code: 404");

    delay(500);   //500ms待機
    M5.update();  // 本体状態更新
    httpClient.end();
    return false;
  }
  if (status_code == -11) {
    // spotifyが動いていないなど
    Serial.println("+ status_code: -11 (timedout)");
    httpClient.end();
    delay(1000);   //500ms待機
    return false;
  }

  Serial.print("+ status_code: ");
  Serial.println(status_code);
  httpClient.end();
  return false;
}

//-------------------------------------------------------------------------------------------
// update_lotate - IMUによる画面の向きの取得
//-------------------------------------------------------------------------------------------
void update_lotate() {
  Serial.println("## update_lotate");
  //    M5.Imu.getGyro(&gyroX,&gyroY,&gyroZ);
  /// M5.Imu.getTemp(&temp);
  M5.Imu.getAccel(&accX, &accY, &accZ);
  int old_angle = rotate_angle;
  //  Serial.printf("gx%.2f, gy%.2f, gz%.2f \n", gyroX,gyroY,gyroZ);
  //  Serial.printf("ax%.2f, ay%.2f, az%.2f \n", accX,accY,accZ);
  //  Serial.printf("Temperature : %.2f C\n", temp);
  float my_roll = atan(accY / accZ) * RAD_TO_DEG;
  float my_pitch = atan(-accX / sqrtf(accY * accY + accZ * accZ)) * RAD_TO_DEG;
  Serial.printf("pitch = %5.1f , roll = %5.1f , z= %f : ", my_pitch, my_roll,accZ);

  if (my_pitch > -18 and my_pitch < 18) {
    if (my_roll > 0) {
      Serial.println("上");
      rotate_angle = 0;  // 上
      if(accZ > 0.980 and my_pitch > 2){
        if(my_roll < 12){
          Serial.println("上→右");
          rotate_angle = 1;  // 右
        }else{
          Serial.println("上→左");
          rotate_angle = 3;  // 左
        }
      }
      if(accZ > 0.980 and my_pitch < -1.4){
        Serial.println("上→右");
        rotate_angle = 1;  // 右
      }
    } else {
      Serial.println("下");
      rotate_angle = 2;  // 下
    }
  } else if (my_pitch > 0) {
    Serial.println("左");
    rotate_angle = 3;  // 左
  } else {
    Serial.println("右");
    rotate_angle = 1;  // 右
  }

}
