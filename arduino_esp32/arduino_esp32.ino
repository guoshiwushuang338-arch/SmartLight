#include <WiFi.h>
#include <Wire.h>
#include <BH1750.h>
#include <PubSubClient.h>
#include <WiFiManager.h>

// 🔵 巴法云 (Bemfa) 配置
const char* mqtt_server = "bemfa.com";
const int mqtt_port = 9501;
const char* mqtt_client_id = "14526d70baca4d7a9fbddafd8501b439";
const char* topic = "myroom002";

BH1750 lightMeter;
WiFiClient espClient;
PubSubClient client(espClient);

// --- 引脚定义 ---
const int ledPin = 2;       
const int radarPin = 14;    

// --- PWM 设置 ---
const int pwmChannel = 0;   
const int pwmFreq = 5000;   
const int pwmRes = 8;       

// --- 变量定义 ---
float lux = 0;              
bool systemState = true;    
bool personDetected = true;
int currentBrightness = 0;  
const int stepSize = 5;     // 仅用于系统关闭时的平滑熄灭

// =========================================================
// PID 闭环控制算法参数
// =========================================================
float Kp = 0.1;   // 比例系数 (P)：决定响应速度
float Ki = 0.01;  // 积分系数 (I)：消除静态误差
float Kd = 0.05;   // 微分系数 (D)：预测未来，防止超调震荡

float targetLux = 400.0; // 绝对目标照度 (取消了以前的区间设定)
float error = 0, lastError = 0, integral = 0;
float smoothedLux = 0;

// =========================================================
// 能耗数学评估模型参数
// =========================================================
const float maxPowerW = 2.5; 
float totalEnergySavedWs = 0; 
float totalEnergyUsedWs = 0;  
unsigned long lastCalcTime = 0;

// --- MQTT 接收指令回调 ---
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  if (msg == "on") { systemState = true; } 
  else if (msg == "off") { systemState = false; }
}

// --- MQTT 断线重连逻辑 ---
void reconnect() {
  while (!client.connected()) {
    if (client.connect(mqtt_client_id)) {
      client.subscribe(topic);
    } else {
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(radarPin, INPUT);

  ledcSetup(pwmChannel, pwmFreq, pwmRes);
  ledcAttachPin(ledPin, pwmChannel);

  Wire.begin();
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

  // --- WiFiManager 自动配网 ---
  WiFiManager wifiManager;
  Serial.println("正在尝试连接保存的 Wi-Fi，或建立热点等待配网...");
  bool res = wifiManager.autoConnect("SmartRoom_Setup");
  if (!res) {
    Serial.println("配网失败或超时，正在重启...");
    delay(3000);
    ESP.restart(); 
  }
  Serial.println("\nWi-Fi 连接成功!");

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) { reconnect(); }
  client.loop(); 

  // --- 传感器读取 ---
  float rawLux = lightMeter.readLightLevel();
  
  // 一階指數平滑濾波 (Exponential Smoothing)
  // 新數據只佔 20% 權重，歷史數據佔 80%，極大消除瞬間閃爍干擾
  if (smoothedLux == 0) smoothedLux = rawLux; // 初始化
  smoothedLux = (smoothedLux * 0.8) + (rawLux * 0.2);
  lux = smoothedLux;
  personDetected = digitalRead(radarPin); 

  // =========================================================
  // 位置式 PID 恒照度计算
  // =========================================================
  if (systemState == true && personDetected == true) {
    // 1. 计算偏差
    error = targetLux - lux;
    
// 加入「死區 (Deadband)」
    // 如果光照誤差在正負 30 Lux 以內，系統認為已經完美達標，強制停止 PID 運算！
    if (abs(error) < 30.0) {
      error = 0; 
      // 清空積分和微分記憶，防止積累誤差突然爆發
      integral = 0;
      lastError = 0;
    } else {
      // 只有誤差大於 30 時，才執行 PID 運算
      integral += error;
      if (integral > 1000) integral = 1000;  // 縮小積分限幅
      if (integral < -1000) integral = -1000;
    
    // 3. 微分项计算
      float derivative = error - lastError;
    
    // 4. PID 输出公式
      float pidOutput = (Kp * error) + (Ki * integral) + (Kd * derivative);
      currentBrightness += (int)pidOutput;
    }
    
    // 5. 输出限幅 (PWM 限制在 0-255)
    if (currentBrightness > 255) currentBrightness = 255;
    if (currentBrightness < 0) currentBrightness = 0;
    
    // 6. 更新误差记忆
    lastError = error;
    
  } else {
    // 系统关闭或无人时，优雅熄灭，并清空 PID 历史记忆
    if (currentBrightness > 0) {
      currentBrightness -= stepSize;
      if (currentBrightness < 0) currentBrightness = 0;
    }
    integral = 0; 
    lastError = 0;
  }
  ledcWrite(pwmChannel, currentBrightness);
  
  // =========================================================
  // 微积分能耗实时监控模型
  // =========================================================
  unsigned long currentTime = millis();
  if (lastCalcTime > 0) {
    // 换算时间差 (秒)
    float deltaTimeS = (currentTime - lastCalcTime) / 1000.0;
    
    // 当前瞬时功率
    float currentPower = maxPowerW * (currentBrightness / 255.0);
    totalEnergyUsedWs += currentPower * deltaTimeS;
    
    // 传统声控灯模式：有人即满载 10W，无人 0W
    float traditionalPower = personDetected ? maxPowerW : 0;
    
    // 累加节约的电能
    totalEnergySavedWs += (traditionalPower - currentPower) * deltaTimeS;
  }
  lastCalcTime = currentTime;

  // --- 数据上报逻辑 (新增能耗数据) ---
  static unsigned long lastSend = 0;
  if (currentTime - lastSend > 1000) { 
    String jsonStr = "{";
    jsonStr += "\"lux\":" + String(lux) + ",";
    jsonStr += "\"state\":" + (systemState ? String("1") : String("0")) + ",";
    jsonStr += "\"person\":" + (personDetected ? String("1") : String("0")) + ",";
    jsonStr += "\"pwm\":" + String(currentBrightness) + ","; 
    // 发送两位小数的节约电能和消耗电能
    jsonStr += "\"saved\":" + String(totalEnergySavedWs, 2) + ","; 
    jsonStr += "\"used\":" + String(totalEnergyUsedWs, 2); 
    jsonStr += "}";
    
    client.publish(topic, jsonStr.c_str());
    lastSend = currentTime;
    
    // 在串口打印出来，方便你调试看效果
    Serial.print("PID亮度: "); Serial.print(currentBrightness);
    Serial.print(" | 累计耗电(J): "); Serial.print(totalEnergyUsedWs);
    Serial.print(" | 累计省电(J): "); Serial.println(totalEnergySavedWs);
  }

  delay(100); 
}