// Include necessary libraries
#include <Arduino.h>          // Main Arduino library
#include <Wire.h>            // I2C communication library
#include <Adafruit_GFX.h>     // Graphics library for displays
#include <Adafruit_SSD1306.h> // SSD1306 OLED display library
#include <DHTesp.h>          // DHT temperature/humidity sensor library
#include <WiFi.h>            // WiFi networking library
#include <PubSubClient.h>     // MQTT client library
#include <ESP32Servo.h>       // ESP32 Servo library
#include <ArduinoJson.h>      // JSON library for data formatting

// Display configuration
#define SCREEN_WIDTH 128      // OLED width in pixels
#define SCREEN_HEIGHT 64      // OLED height in pixels
#define SCREEN_ADDRESS 0x3C   // I2C address of OLED display

// NTP (Network Time Protocol) configuration
#define NTP_SERVER     "pool.ntp.org"  // NTP server for time synchronization
#define UTC_OFFSET     19800           // Default UTC+5:30 (5 hours and 30 minutes in seconds)
#define UTC_OFFSET_DST 0               // Daylight savings offset (not used)

// Pin definitions
#define BUZZER 5       // Buzzer pin for alarms
#define LED_1 15       // LED indicator pin
#define PB_CANCEL 25   // Cancel button pin
#define PB_OK 32       // OK button pin
#define PB_UP 33       // Up button pin
#define PB_DOWN 35     // Down button pin
#define DHTPIN 12      // DHT sensor pin
#define LDR_PIN 34      // Light Dependent Resistor pin
#define SERVO_PIN 13    // Servo motor pin

// Create display and sensor objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); // OLED display object
DHTesp dhtSensor;      // Temperature/humidity sensor object
Servo windowServo;     // Servo motor object for shaded window

// Global time variables
int day=0;             // Current day
int month=0;           // Current month
int year=0;            // Current year
int days=0;            // Day of week (0-6)
int hours=0;           // Current hour
int minutes=0;         // Current minute
int seconds=0;         // Current second

// MQTT and WiFi configuration
const char* ssid = "Wokwi-GUEST";            // WiFi SSID
const char* password = "";                   // WiFi password
const char* mqtt_server = "broker.hivemq.com"; // MQTT broker address
const int mqtt_port = 1883;                    // MQTT broker port
const char* clientID = "medibox_client";      // MQTT client ID

// MQTT topics for publishing and subscribing
const char* lightTopic = "medibox/light";           // Topic to publish light readings
const char* tempTopic = "medibox/temperature";      // Topic to publish temperature readings
const char* angleParamsTopic = "medibox/params";    // Topic to receive angle parameters
const char* intervalsTopic = "medibox/intervals";   // Topic to receive sampling intervals

// Light intensity monitoring variables
unsigned long lastSampleTime = 0;        // Last light sampling time
unsigned long lastSendTime = 0;          // Last data sending time
int samplingInterval = 5000;             // Default sampling interval (5 seconds in ms)
int sendingInterval = 12000;            // Default sending interval (2 minutes in ms)
float lightReadings[24];                 // Array to store light readings (max 24 for 2 min at 5 sec)
int readingIndex = 0;                    // Current index in readings array
int totalReadings = 0;                   // Total number of readings taken

// Servo motor control variables
float theta_offset = 30.0;               // Minimum angle in degrees (default: 30)
float gamma_factor = 0.75;                     // Controlling factor (default: 0.75)
float T_med = 30.0;                      // Ideal storage temperature (default: 30°C)
int servoAngle = 30;                     // Current servo angle


// Time zone configuration
int utc_offset = UTC_OFFSET;  // Current UTC offset in seconds

unsigned long tempSendInterval = 7000;      // Temperature sending interval (1 second in ms)
unsigned long lastTempSendTime = 0;         // Last temperature sending time

// Timing variables
unsigned long timeNow=0;      // Current time marker
unsigned long timeLast=0;     // Previous time marker

// Alarm system variables
bool alarms_enabled[]={false,false};  // Alarm enable status
int n_alarms=2;                       // Number of alarms
int alarm_hours[]={0,0};              // Alarm hours
int alarm_minutes[]={0,0};            // Alarm minutes
bool alarm_triggered[]={false,false}; // Alarm trigger status
unsigned long snooze_times[]={0,0};   // Snooze end times
int snooze_duration = 5;              // Snooze duration in minutes

// Buzzer notes configuration
int n_notes=8;                        // Number of musical notes
int C=262;    // Note frequencies
int D=294;
int E=330;
int F=349;
int G=392;
int A=440;
int B=494;
int C_H=523;
int notes[]={C,D,E,F,G,A,B,C_H};  // Array of notes for alarm melody

// Menu system variables
int current_mode=0;               // Current selected mode
int max_modes=5;                  // Number of available modes
String modes[]={"1 - Set Time Zone","2 - Set Alarm 1","3 - Set Alarm 2","4 - View Active Alarms","5 - Delete an Alarm"};


// MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// Forward declarations
void print_line(String text, int column, int row, int text_size);
float readLightIntensity();
int calculateServoAngle(float lightIntensity, float temperature);
void updateServoPosition(int angle);
void set_offset_time();
void set_alarm(int alarm);
void view_active_alarms();
void delete_alarm();
int wait_for_button_press();
void publishTemperatureData(float temperature);
void publishLightData(float lightIntensity);
void check_temp();
void run_mode(int mode);
void go_to_menu();
void print_time_now();
void update_time();
void ring_alarm(int alarm_index);
void update_time_with_check_alarm();
void reconnect();
void callback(char* topic, byte* payload, unsigned int length);
float calculateAverageLightIntensity();

// Function to display text on OLED
void print_line(String text,int column,int row,int text_size)
{
  display.setTextSize(text_size);         // Set text size
  display.setTextColor(SSD1306_WHITE);    // Set text color (white)
  display.setCursor(column, row);         // Set cursor position
  display.println(text);                  // Print text
  display.display();                      // Update display
}

float readLightIntensity() {
  int rawValue = analogRead(LDR_PIN);
  
  // Map analog reading (0-4095 for ESP32) to 0-1 range
  // For LDR, lower values typically mean higher light intensity
  // and higher values mean lower light intensity, so we invert the mapping
  
  // You may need to calibrate these min/max values based on your environment
  int minReading = 32;    // Value in total darkness
  int maxReading = 4063; // Value in brightest light
  
  // Normalize to 0-1 range (0 = darkness, 1 = brightest light)
  float normalizedValue = 1.0 - ((float)(rawValue - minReading) / (maxReading - minReading));
  
  // Constrain to ensure value stays in 0-1 range
  normalizedValue = constrain(normalizedValue, 0.0, 1.0);
  
  return normalizedValue;
}

// Function to calculate average light intensity
float calculateAverageLightIntensity() {
  if (totalReadings == 0) {
    return 0;
  }
  
  float sum = 0;
  int count = min(totalReadings, 24); // Limit to array size (24 samples = 2 min at 5 sec intervals)
  
  for (int i = 0; i < count; i++) {
    sum += lightReadings[i];
  }
  
  return sum / count;
}

int calculateServoAngle(float lightIntensity, float temperature) {
  // θ = θoffset + (180 - θoffset) × I × γ × ln(ts/tu) × (T/Tmed)
  float ts = samplingInterval / 1000.0; // Convert to seconds
  float tu = sendingInterval / 1000.0;  // Convert to seconds
  
  // Ensure we don't take log of negative or zero
  float ratio = ts / tu;
  if (ratio <= 0) ratio = 0.01;
  
  // Calculate the angle based on the provided formula
  float angle = theta_offset + 
              (180 - theta_offset) * 
              lightIntensity * 
              gamma_factor * 
              log(ratio) * 
              (temperature / T_med);
  
  // Debug information
  Serial.println("Angle calculation parameters:");
  Serial.print("  theta_offset: "); Serial.println(theta_offset);
  Serial.print("  light intensity: "); Serial.println(lightIntensity);
  Serial.print("  gamma_factor: "); Serial.println(gamma_factor);
  Serial.print("  ts/tu ratio: "); Serial.println(ratio);
  Serial.print("  log(ratio): "); Serial.println(log(ratio));
  Serial.print("  temperature: "); Serial.println(temperature);
  Serial.print("  T_med: "); Serial.println(T_med);
  Serial.print("  T/T_med: "); Serial.println(temperature / T_med);
  Serial.print("  Calculated angle: "); Serial.println(angle);
  
  // Constrain angle to valid servo range
  angle = constrain(angle, 0, 180);
  Serial.print("  Constrained angle: "); Serial.println(angle);
  
  return (int)angle;
}

// Function to update servo position
void updateServoPosition(int angle) {
  // Check if there's a significant change to avoid unnecessary servo movements
  static int lastAngle = -1;
  if (abs(angle - lastAngle) >= 10) { // Only move if angle changed by at least 2 degrees
    windowServo.write(angle);
    lastAngle = angle;
    Serial.print("Servo angle updated to: ");
    Serial.println(angle);
  }
}
// Function to update servo position



// Function to reconnect to MQTT broker
void reconnect() {
  // Loop until we're reconnected
  int retryCount = 0;
  while (!client.connected() && retryCount < 5) { // Limit retry attempts
    retryCount++;
    display.clearDisplay();
    print_line("Connecting to MQTT...", 0, 0, 1);
    print_line("Attempt " + String(retryCount), 0, 16, 1);
    
    Serial.print("Attempting MQTT connection... ");
    
    // Create a random client ID
    String clientId = "medibox_client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      display.clearDisplay();
      print_line("Connected to MQTT", 0, 0, 1);
      
      // Subscribe to topics for parameter updates
      client.subscribe(angleParamsTopic);
      client.subscribe(intervalsTopic);
      delay(500);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5s...");
      display.clearDisplay();
      print_line("Failed to connect", 0, 0, 1);
      print_line("Error code: " + String(client.state()), 0, 16, 1);
      print_line("Retrying in 5s...", 0, 32, 1);
      delay(5000);
    }
  }
}



// MQTT message callback function to handle parameter updates
void callback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  
  // Parse JSON
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  // Check for parsing errors
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Handle different topics
  if (strcmp(topic, angleParamsTopic) == 0) {
    // Update servo control parameters
    if (doc.containsKey("theta_offset")) {
      theta_offset = doc["theta_offset"];
      Serial.print("Updated theta_offset: ");
      Serial.println(theta_offset);
    }
    if (doc.containsKey("gamma")) {
      gamma_factor = doc["gamma"];
      Serial.print("Updated gamma_factor: ");
      Serial.println(gamma_factor);
    }
    if (doc.containsKey("T_med")) {
      T_med = doc["T_med"];
      Serial.print("Updated T_med: ");
      Serial.println(T_med);
    }
    
    // Show confirmation on display
    display.clearDisplay();
    print_line("Parameters updated:", 0, 0, 1);
    print_line("Min Angle: " + String(theta_offset), 0, 16, 1);
    print_line("Factor: " + String(gamma_factor), 0, 32, 1);
    print_line("Ideal Temp: " + String(T_med), 0, 48, 1);
    
    // Update servo position immediately with new parameters
    float intensity = readLightIntensity();
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    servoAngle = calculateServoAngle(intensity, data.temperature);
    updateServoPosition(servoAngle);
    
    delay(2000);
  }
  else if (strcmp(topic, intervalsTopic) == 0) {
    // Update sampling and sending intervals
    if (doc.containsKey("sampling")) {
      int newSampling = doc["sampling"].as<int>();
      
      // Ensure sampling interval is at least 1 second and at most 60 seconds
      newSampling = constrain(newSampling, 1, 60);
      samplingInterval = newSampling * 1000; // Convert to milliseconds
      Serial.print("Updated samplingInterval: ");
      Serial.println(samplingInterval);
    }
    
    if (doc.containsKey("sending")) {
      int newSending = doc["sending"].as<int>();
      
      // Ensure sending interval is at least 10 seconds and reasonable
      newSending = constrain(newSending, 10, 600); // 10 seconds to 10 minutes
      sendingInterval = newSending * 1000; // Convert to milliseconds
      Serial.print("Updated sendingInterval: ");
      Serial.println(sendingInterval);
    }
    
    // Reset light readings array when intervals change
    for (int i = 0; i < 24; i++) {
      lightReadings[i] = 0;
    }
    readingIndex = 0;
    totalReadings = 0;
    
    // Update servo position with new interval parameters
    float intensity = readLightIntensity();
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    servoAngle = calculateServoAngle(intensity, data.temperature);
    updateServoPosition(servoAngle);
    
    // Show confirmation on display
    display.clearDisplay();
    print_line("Intervals updated:", 0, 0, 1);
    print_line("Sample: " + String(samplingInterval/1000) + "s", 0, 16, 1);
    print_line("Send: " + String(sendingInterval/1000) + "s", 0, 32, 1);
    delay(2000);
  }
}
// Function to publish light and temperature data to MQTT
void publishTemperatureData(float temperature) {
  // Create JSON document
  StaticJsonDocument<200> doc;
  
  // Add temperature data
  doc["temperature"] = temperature;
  doc["timestamp"] = millis();
  
  // Serialize JSON to string
  char buffer[100];
  serializeJson(doc, buffer);
  
  // Publish to temperature topic
  client.publish(tempTopic, buffer);
}

void publishLightData(float lightIntensity) {
  // Create JSON document
  StaticJsonDocument<200> doc;
  
  // Add light intensity data (ensuring it's in 0-1 range)
  doc["intensity"] = constrain(lightIntensity, 0.0, 1.0);
  doc["timestamp"] = millis();
  doc["sampling_interval"] = samplingInterval / 1000; // Convert to seconds
  doc["sending_interval"] = sendingInterval / 1000;   // Convert to seconds
  
  // Serialize JSON to string
  char buffer[100];
  serializeJson(doc, buffer);
  
  // Debug print
  Serial.print("Publishing to topic: ");
  Serial.println(lightTopic);
  Serial.print("Message: ");
  Serial.println(buffer);
  
  // Publish to light topic and check result
  boolean success = client.publish(lightTopic, buffer);
  if (success) {
    Serial.println("Publish successful");
  } else {
    Serial.println("Publish failed");
  }
}


void run_mode(int mode)
{
  if(mode==0)  // Set time zone
  {
    set_offset_time();
  }
  else if(mode ==1 || mode ==2)  // Set alarm 1 or 2
  {
    set_alarm(mode-1);
  }
  else if(mode==3)  // View active alarms
  {
    view_active_alarms();
  }
  else if(mode==4)  // Delete alarm
  {
    delete_alarm();
  }
}

// Function to display and navigate menu
void go_to_menu()
{
  while(digitalRead(PB_CANCEL) == HIGH)
  {
    display.clearDisplay();
    print_line("Displaying current mode...",0,0,1);
    print_line(modes[current_mode],0,30,1);

    int pressed=wait_for_button_press();

    if(pressed == PB_UP)  // Next mode
    {
      delay(200);
      current_mode+=1;
      current_mode=current_mode % max_modes;
    }
    else if(pressed == PB_DOWN)  // Previous mode
    {
      delay(200);
      current_mode-=1;
      if(current_mode<0)
      {
        current_mode=max_modes-1;
      }
    }
    else if(pressed == PB_OK)  // Select mode
    {
      delay(200);
      run_mode(current_mode);
    }
    else if(pressed == PB_CANCEL)  // Exit menu
    {
      delay(200);
      break;
    }
  }
}


void print_time_now()
{
  display.clearDisplay();
  
  // Display date in format DD/MM/YY
  String dateStr = String(day) + "/" + String(month) + "/" + String(year % 100);
  print_line(dateStr, 0, 0, 1);
  
  // Display time in HH:MM:SS format
  print_line(String(hours),0,16,2);
  print_line(":",20,16,2);
  print_line(String(minutes),30,16,2);
  print_line(":",50,16,2);
  print_line(String(seconds),60,16,2);
  
  // Display current timezone
  int tz_hour = utc_offset / 3600;
  int tz_min = (utc_offset % 3600) / 60;
  String tzStr = "UTC";
  if (tz_hour >= 0) tzStr += "+"; 
  tzStr += String(tz_hour);
  if (tz_min > 0) {
    tzStr += ":";
    if (tz_min < 10) tzStr += "0";
    tzStr += String(tz_min);
  }
  print_line(tzStr, 0, 32, 1);
}

// Function to update time from NTP server
void update_time()
{
  struct tm timeinfo;
  getLocalTime(&timeinfo);  // Get current time

  // Extract hours, minutes, seconds
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  hours=atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute,3, "%M", &timeinfo);
  minutes=atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond,3, "%S", &timeinfo);
  seconds=atoi(timeSecond);

  // Extract day, month, year
  char timeDay[3];
  strftime(timeDay,3, "%d", &timeinfo);
  day=atoi(timeDay);

  char timeMonth[3];
  strftime(timeMonth,3, "%m", &timeinfo);
  month=atoi(timeMonth);

  char timeYear[5];
  strftime(timeYear,5, "%Y", &timeinfo);
  year=atoi(timeYear);
  
  // Extract day of week (0-6, Sunday=0)
  char dayOfWeek[3];
  strftime(dayOfWeek,3, "%w", &timeinfo);
  days=atoi(dayOfWeek);
}

// Function to sound the alarm
void ring_alarm(int alarm_index)
{
  display.clearDisplay();
  print_line("MEDICINE TIME!",0,0,2);
  print_line("UP: Snooze  DOWN: Stop",0,40,1);

  digitalWrite(LED_1, HIGH);  // Turn on LED

  bool break_happened = false;
  bool snooze_selected = false;
  
  // Alarm loop
  while(break_happened == false && digitalRead(PB_CANCEL)==HIGH){
    for (int i=0;i<n_notes;i++){
      // Check for stop button
      if (digitalRead(PB_DOWN)==LOW)
      {
        delay(200);
        break_happened = true;
        break;
      }
      
      // Check for snooze button
      if (digitalRead(PB_UP)==LOW)
      {
        delay(200);
        snooze_selected = true;
        break_happened = true;
        break;
      }
      
      // Play alarm tone
      tone(BUZZER,notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  digitalWrite(LED_1, LOW);  // Turn off LED
  
  // Handle snooze or stop
  if (snooze_selected) {
    // Set snooze time (current time + snooze duration)
    snooze_times[alarm_index] = millis() + (snooze_duration * 60 * 1000);
    alarm_triggered[alarm_index] = false;
    
    display.clearDisplay();
    print_line("Snooze for " + String(snooze_duration) + " min",0,0,1);
    delay(1000);
  } else {
    // Mark alarm as handled
    alarm_triggered[alarm_index] = true;
    snooze_times[alarm_index] = 0;
  }
  
  display.clearDisplay();
}

// Function to update time and check alarms
void update_time_with_check_alarm()
{
   update_time();      // Get current time
   print_time_now();   // Display time

   // Check all alarms
   for(int i=0;i<n_alarms;i++)
   {
     // Check if alarm time matches current time
     bool time_match = (alarm_hours[i]==hours && alarm_minutes[i]==minutes && seconds < 10);
     
     // Check if snooze time has expired
     bool snooze_expired = (snooze_times[i] > 0 && millis() >= snooze_times[i]);
     
     // Trigger alarm if conditions met
     if (alarms_enabled[i] && !alarm_triggered[i] && (time_match || snooze_expired))
     {
       if (snooze_expired) {
         snooze_times[i] = 0;  // Reset snooze time
       }
       
       ring_alarm(i);  // Sound the alarm
     }
   }
}

// Function to wait for button press
int wait_for_button_press()
{
  while(true)
  {
    if(digitalRead(PB_UP) == LOW)
    {
      delay(200);  // Debounce delay
      return PB_UP;
    }
    else if(digitalRead(PB_DOWN) == LOW)
    {
      delay(200);
      return PB_DOWN;
    }
    else if(digitalRead(PB_OK) == LOW)
    {
      delay(200);
      return PB_OK;
    }
    else if(digitalRead(PB_CANCEL) == LOW)
    {
      delay(200);
      return PB_CANCEL;
    }

    update_time();  // Keep time updated while waiting
  }
}

// Function to set timezone offset
void set_offset_time()
{
  // Setting hours offset from UTC
  int temp_hour = utc_offset / 3600;

  while(true)
  {
    display.clearDisplay();
    print_line("Offset Hours: " + String(temp_hour), 0, 0, 1);
    print_line("UP/DOWN to change", 0, 12, 1);
    print_line("OK to confirm", 0, 24, 1);
    print_line("CANCEL to exit", 0, 36, 1);
    
    int pressed = wait_for_button_press();

    if(pressed == PB_UP)
    {
      delay(200);
      temp_hour = (temp_hour + 1) % 13; // Cycle through -12 to +12
    }
    else if(pressed == PB_DOWN)
    {
      delay(200);
      temp_hour -= 1;
      if(temp_hour < -12)
      {
        temp_hour = 12;
      }
    }
    else if(pressed == PB_OK)
    {
      delay(200);
      break;
    }
    else if(pressed == PB_CANCEL)
    {
      delay(200);
      return; // Exit without saving
    }
  }

  // Setting minutes offset
  int temp_minute = (utc_offset % 3600) / 60;

  while(true)
  {
    display.clearDisplay();
    print_line("Offset Minutes: " + String(temp_minute), 0, 0, 1);
    print_line("UP/DOWN to change", 0, 12, 1);
    print_line("OK to confirm", 0, 24, 1);
    print_line("CANCEL to exit", 0, 36, 1);
    
    int pressed = wait_for_button_press();

    if(pressed == PB_UP)
    {
      delay(200);
      temp_minute = (temp_minute + 15) % 60; // Increment by 15 minutes
    }
    else if(pressed == PB_DOWN)
    {
      delay(200);
      temp_minute -= 15;
      if(temp_minute < 0)
      {
        temp_minute = 45;
      }
    }
    else if(pressed == PB_OK)
    {
      delay(200);
      break;
    }
    else if(pressed == PB_CANCEL)
    {
      delay(200);
      return; // Exit without saving
    }
  }
 
  // Calculate new offset in seconds
  int new_offset = (temp_hour * 3600) + (temp_minute * 60);
  
  // Apply new time zone
  utc_offset = new_offset;
  
  // Update time configuration
  configTime(utc_offset, UTC_OFFSET_DST, NTP_SERVER);
  
  // Show confirmation
  display.clearDisplay();
  print_line("Time Zone Updated", 0, 0, 1);
  
  // Display new timezone
  String tzStr = "UTC";
  if (temp_hour >= 0) tzStr += "+"; 
  tzStr += String(temp_hour);
  if (temp_minute > 0) {
    tzStr += ":";
    if (temp_minute < 10){
      tzStr += "0";
      tzStr += String(temp_minute);
    }
  }
  print_line(tzStr, 0, 16, 2);
  
  delay(2000);
}

// Function to set an alarm
void set_alarm(int alarm)
{
  // Set alarm hour
  int temp_hour=alarm_hours[alarm];

  while(true)
  {
    display.clearDisplay();
    print_line("Enter hour: "+String(temp_hour),0,0,1);

    int pressed=wait_for_button_press();

    if(pressed == PB_UP)
    {
      delay(200);
      temp_hour+=1;
      temp_hour=temp_hour % 24;  // Wrap around after 23
    }
    else if(pressed == PB_DOWN)
    {
      delay(200);
      temp_hour-=1;
      if(temp_hour<0)
      {
        temp_hour=23;
      }
    }
    else if(pressed == PB_OK)
    {
      delay(200);
      alarm_hours[alarm]=temp_hour;
      break;
    }
    else if(pressed == PB_CANCEL)
    {
      delay(200);
      break;
    }
  }

  // Set alarm minute
  int temp_minute=alarm_minutes[alarm];

  while(true)
  {
    display.clearDisplay();
    print_line("Enter minute: "+String(temp_minute),0,0,1);

    int pressed=wait_for_button_press();

    if(pressed == PB_UP)
    {
      delay(200);
      temp_minute+=1;
      temp_minute=temp_minute % 60;
    }
    else if(pressed == PB_DOWN)
    {
      delay(200);
      temp_minute-=1;
      if(temp_minute<0)
      {
        temp_minute=59;
      }
    }
    else if(pressed == PB_OK)
    {
      delay(200);
      alarm_minutes[alarm]=temp_minute;
      break;
    }
    else if(pressed == PB_CANCEL)
    {
      delay(200);
      break;
    }
  }
 
  // Confirm alarm set
  display.clearDisplay();
  alarms_enabled[alarm]=true;
  alarm_triggered[alarm]=false;
  snooze_times[alarm]=0;
  print_line("Alarm is set",0,0,2);
  delay(1000);
}

// Function to view active alarms
void view_active_alarms()
{
   int row=0;
   display.clearDisplay();
   
   bool any_alarms = false;
   // Display all enabled alarms
   for(int i=0;i<n_alarms;i++)
   {
      if(alarms_enabled[i]==true)
      {
        any_alarms = true;
        // Format alarm time with leading zero for minutes
        String alarm_time = String(alarm_hours[i]);
        alarm_time += ":";
        if(alarm_minutes[i] < 10) alarm_time += "0";
        alarm_time += String(alarm_minutes[i]);
        
        print_line("Alarm " + String(i+1) + ": " + alarm_time, 0, row, 1);
        row+=10;
      }
   }
   
   // Show message if no alarms set
   if (!any_alarms) {
     print_line("No active alarms", 0, 20, 1);
   }
   
   print_line("Press any button", 0, 50, 1);
   wait_for_button_press();
}

// Function to delete an alarm
void delete_alarm()
{
  int alarm = 0;  // Start with first alarm
  
  while (true) {
    display.clearDisplay();
    print_line("Delete alarm " + String(alarm + 1) + "?", 0, 0, 2);
    print_line("OK to delete", 0, 40, 1);
    print_line("UP/DOWN to select", 0, 50, 1);
    print_line("CANCEL to exit", 0, 60, 1);
    
    int pressed = wait_for_button_press();
    
    if (pressed == PB_UP) {
      delay(200);
      alarm = (alarm + 1) % n_alarms;  // Cycle through alarms
    } else if (pressed == PB_DOWN) {
      delay(200);
      alarm = (alarm - 1 + n_alarms) % n_alarms;
    } else if (pressed == PB_OK) {
      delay(200);
      // Clear alarm settings
      alarms_enabled[alarm] = false;
      alarm_hours[alarm] = 0;
      alarm_minutes[alarm] = 0;
      alarm_triggered[alarm] = false;
      snooze_times[alarm] = 0;
      
      display.clearDisplay();
      print_line("Alarm " + String(alarm + 1) + " deleted", 0, 0, 2);
      delay(1000);
    } else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
  
  display.clearDisplay();
}

// Function to check temperature and humidity
void check_temp()
{
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  
  // Check for and display warnings if needed
  if(data.temperature > 32)
  {
    print_line("TEMP HIGH!", 0, 50, 1);
  }
  else if(data.temperature < 24)
  {
    print_line("TEMP LOW!", 0, 50, 1);
  }
  else {
    print_line("HEALTHY TEMP ", 0, 50, 1);
  }

  if(data.humidity > 80)
  {
    print_line("HUMIDITY HIGH!", 0, 40, 1);
  }
  else if(data.humidity < 65)
  {
    print_line("HUMIDITY LOW!", 0, 40, 1);
  }
  else {
    print_line("HEALTHY HUMIDITY", 0, 40, 1);
  }
}
// Add these after your includes but before setup()


// Function to run selected mode
void setup() {
  // Initialize pins
  pinMode(BUZZER, OUTPUT);    // Set buzzer as output
  pinMode(LED_1, OUTPUT);     // Set LED as output
  pinMode(PB_CANCEL, INPUT);  // Set buttons as inputs
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);
  pinMode(LDR_PIN, INPUT);  

  // Initialize DHT sensor
  dhtSensor.setup(DHTPIN,DHTesp::DHT22);

  // Servo initialization in setup function
// Add this to the setup function after the pin initializations

// Initialize servo motor with proper configurations
pinMode(SERVO_PIN, OUTPUT);
windowServo.setPeriodHertz(50); // Standard 50Hz for servos
windowServo.attach(SERVO_PIN, 500, 2400); // Adjust min/max pulse width if needed
delay(100); // Short delay for servo to initialize
windowServo.write(theta_offset); // Set initial position to minimum angle
Serial.print("Servo initialized at angle: ");
Serial.println(theta_offset);

// Initialize parameters for servo control with default values
Serial.println("Initializing servo control parameters:");
Serial.print("  theta_offset (min angle): "); Serial.println(theta_offset);
Serial.print("  gamma_factor: "); Serial.println(gamma_factor);
Serial.print("  T_med (ideal temp): "); Serial.println(T_med);

  // Start serial communication with higher baud rate for better debugging
  Serial.begin(115200);
  Serial.println("MediBox starting...");

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("failed to start SSD1306 OLED"));
    for (;;);  // Infinite loop if display fails
  }

  display.display();  // Show initial display buffer
  delay(2000);        // Pause for 2 seconds

  // Connect to WiFi
  Serial.print("Connecting to WiFi...");
  WiFi.begin("Wokwi-GUEST", "", 6);  // Connect to WiFi network
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    display.clearDisplay();
    print_line("Connecting to WIFI",0,0,2);  // Show connection status
  }

  // WiFi connected
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  display.clearDisplay();
  print_line("Connected to WIFI",0,0,2);

  // Initialize random seed for client ID generation
  randomSeed(micros());

  // Set up MQTT client with explicit server and port
  Serial.println("Setting up MQTT client...");
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Print MQTT configuration
  Serial.print("MQTT Server: ");
  Serial.println(mqtt_server);
  Serial.print("MQTT Port: ");
  Serial.println(mqtt_port);
  Serial.print("Light Topic: ");
  Serial.println(lightTopic);
  Serial.print("Temperature Topic: ");
  Serial.println(tempTopic);

  // Show welcome message
  display.clearDisplay();
  print_line("Welcome to Medibox!",10,20,2);
  delay(2000);
  display.clearDisplay();

  // Configure NTP time
  configTime(utc_offset, UTC_OFFSET_DST, NTP_SERVER);
  
  // Initial MQTT connection
  if (!client.connected()) {
    reconnect();
  }
  
  // Debug message for startup
  Serial.println("Setup complete");
}


void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost, reconnecting...");
    WiFi.reconnect();
    delay(1000);
  }
  
  // Ensure MQTT connection
  if (!client.connected()) {
    Serial.println("MQTT connection lost, reconnecting...");
    reconnect();
  }
  client.loop();
  
  // Update time and check alarms
  update_time_with_check_alarm();
  
  // Check if menu button pressed
  if(digitalRead(PB_OK) == LOW)   
  {
    delay(200);
    go_to_menu();  // Enter menu system
  }
  
  // Check temperature and humidity
  check_temp();
  
  // Get current time
  unsigned long currentTime = millis();

// Sample from the loop function to update servo motor properly
// This should be placed in the loop function where the light sensing occurs

// Sample light intensity at regular intervals (default: 5 seconds)
if (currentTime - lastSampleTime >= samplingInterval) {
  lastSampleTime = currentTime;

  // Read light intensity from LDR (0-1 range)
  float intensity = readLightIntensity();
  
  // Debug print the raw reading
  Serial.print("Light reading: ");
  Serial.println(intensity);

  // Store reading in circular buffer
  lightReadings[readingIndex] = intensity;
  readingIndex = (readingIndex + 1) % 24; // Wrap around after 24 readings
  if (totalReadings < 24) totalReadings++;

  // Read temperature
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  
  // Calculate and update servo position
  servoAngle = calculateServoAngle(intensity, data.temperature);
  updateServoPosition(servoAngle);
  
  // Update display with sensor information
  display.clearDisplay();
  // Format light intensity to 2 decimal places for cleaner display
  char lightStr[10];
  sprintf(lightStr, "%.2f", intensity);
  print_line("Light: " + String(lightStr), 0, 0, 1);
  print_line("Temp: " + String(data.temperature) + "C", 0, 10, 1);
  print_line("Humid: " + String(data.humidity) + "%", 0, 20, 1);
  print_line("Servo: " + String(servoAngle) + " deg", 0, 30, 1);
}

  // Send temperature data every 1 second
  if (currentTime - lastTempSendTime >= tempSendInterval) {
    lastTempSendTime = currentTime;
    
    // Get current temperature
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    
    // Publish temperature data to MQTT
    publishTemperatureData(data.temperature);
  }

  // Send averaged light data at regular intervals (default: 2 minutes)
  if (currentTime - lastSendTime >= sendingInterval) {
    lastSendTime = currentTime;
    
    // Calculate average light intensity
    float avgIntensity = calculateAverageLightIntensity();
    
    Serial.print("Average light intensity to publish: ");
    Serial.println(avgIntensity);
    
    // Publish light data to MQTT
    publishLightData(avgIntensity);
    
    // Show notification on display
    display.clearDisplay();
    char avgStr[10];
    sprintf(avgStr, "%.2f", avgIntensity);
    print_line("Avg Light: " + String(avgStr), 0, 0, 1);
    print_line("Data sent to MQTT", 0, 12, 1);
    print_line("Sample: " + String(samplingInterval/1000) + "s", 0, 24, 1);
    print_line("Send: " + String(sendingInterval/1000) + "s", 0, 36, 1);
    delay(1000);
  }
  
  delay(50); // Small delay for stability
}