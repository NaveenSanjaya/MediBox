// include libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// Define OLED parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3c

#define NTP_SERVER "pool.ntp.org"
#define UTC_OFFSET_DST 0

#define BUZZER 5
#define LED_1 15
#define LED_2 2
#define PB_CANCEL 34
#define PB_DOWN 35
#define PB_OK 32
#define PB_UP 26
#define PB_SNOOZE 25
#define DHTPIN 12

#define LDR 33
#define SERVO 13

#define MQTT_SERVER "broker.hivemq.com"
#define MQTT_PORT 1883

// Declare Objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;
Servo servo;

WiFiClient espClient;
PubSubClient mqttclient(espClient);

// Function Declarations
void print_line(String text, int column, int row, int text_size);
void print_line_centered(String text, int row, int text_size);
void print_time_now(void);
void update_time(void);
void ring_alarm(void);
void update_time_with_check_alarm(void);
void go_to_menu();
void set_time();
void set_time_zone();
void set_alarm(int alarm);
void run_mode(int mode);
void view_active_alarms();
void delete_alarm();
void enable_disable_alarms();

void mqttCallback(char *topic, byte *payload, unsigned int length);
void connectToMQTT();
void publishData(float temperature, float humidity);
void sampleLDR();
void computeServoAngle(float lightValue, float temperature);

// Global Variables
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;
String date;

int UTC_OFFSET = 5 * 60 * 60 + 30 * 60; // 5 hours 30 minutes = 5*60*60 + 30*60 = 19800 seconds
int UTC_OFFSET_HOURS = 5;
int UTC_OFFSET_MINUTES = 30;

unsigned long timeNow = 0;
unsigned long timeLast = 0;

bool alarms_enabled = true;
int n_alarms = 2;
int alarm_hours[] = {8, 13};
int alarm_minutes[] = {40, 0};
bool alarm_triggered[] = {false, false};

int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};
int n_notes = 8;

unsigned long lastSampleTime = 0;
unsigned long lastSendTime = 0;

float lightSum = 0;
int sampleCount = 0;

float latestTemperature = 0.0;
float latestIntensity = 0.0;

int ts = 5;  // sampling interval (in seconds)
int tu = 10; // sending interval (in seconds)

float thetaOfset = 30;
float gammaValue = 0.75;
float Tmed = 30.0;

int current_mode = 0;
int max_modes = 6;
String modes[] = {"1- \nSet Time \nZone", "2- \nSet Alarm 1", "3- \nSet Alarm 2", "4- \nEnable/\nDisable \nAlarms", "5- \nView \nActive \nAlarms", "6- \nDelete \nAlarms"};

struct TempHum
{
    float temperature;
    float humidity;
};

void setup()
{
    // put your setup code here, to run once:
    pinMode(BUZZER, OUTPUT);
    pinMode(LED_1, OUTPUT);
    pinMode(LED_2, OUTPUT);
    pinMode(PB_SNOOZE, INPUT);
    pinMode(PB_CANCEL, INPUT);
    pinMode(PB_DOWN, INPUT);
    pinMode(PB_OK, INPUT);
    pinMode(PB_UP, INPUT);
    pinMode(LDR, INPUT);

    dhtSensor.setup(DHTPIN, DHTesp::DHT22);
    servo.attach(SERVO);

    Serial.begin(115200);

    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ;
    }

    display.clearDisplay();

    print_line_centered("Welcome", 10, 2);
    print_line_centered("to Medibox!", 30, 1.5);
    display.display();
    delay(500);
    display.clearDisplay();

    // show the display buffer on the screen. you MUST call display() after
    // drawing commands to make them visible on screen!

    WiFi.begin("Wokwi-GUEST", "", 6);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(250);
        display.clearDisplay();
        print_line_centered("Connecting to WiFi", 20, 1);
        display.display();
        delay(500);
    }

    display.clearDisplay();
    print_line_centered("Connected to WiFi", 20, 1);
    display.display();
    delay(500);

    // Connect to MQTT broker
    mqttclient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttclient.setCallback(mqttCallback);

    configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

    // clear the buffer
    display.clearDisplay();
}

void loop()
{

    if (!mqttclient.connected())
    {
        connectToMQTT();
    }
    mqttclient.loop();

    update_time_with_check_alarm();

    if (digitalRead(PB_OK) == LOW)
    {
        delay(200);
        go_to_menu();
    }

    unsigned long now = millis();

    if (now - lastSampleTime >= ts * 1000)
    {
        sampleLDR();
        lastSampleTime = now;
    }

    TempHum th = check_temp();

    if (now - lastSendTime >= tu * 1000)
    {
        // Update servo angle only once per send cycle, with latest values
        computeServoAngle(latestIntensity, latestTemperature);
        publishData(th.temperature, th.humidity);
        lastSendTime = now;
    }
}

//
// Utility Functions
//
void print_line(String text, int column, int row, int text_size)
{
    display.setTextSize(text_size);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(column, row);
    display.println(text);

    display.display();
}

void print_line_centered(String text, int row, int text_size)
{
    int16_t x1, y1;
    uint16_t w, h;

    display.setTextSize(text_size);
    display.getTextBounds(text, 0, row, &x1, &y1, &w, &h);

    int column = (SCREEN_WIDTH - w) / 2; // Calculate the center column
    display.setCursor(column, row);
    display.setTextColor(SSD1306_WHITE);
    display.println(text);
}

int wait_for_button_press()
{
    while (true)
    {
        if (digitalRead(PB_UP) == LOW)
        {
            delay(200); // Debounce
            return PB_UP;
        }
        if (digitalRead(PB_DOWN) == LOW)
        {
            delay(200);
            return PB_DOWN;
        }
        if (digitalRead(PB_OK) == LOW)
        {
            delay(200);
            return PB_OK;
        }
        if (digitalRead(PB_CANCEL) == LOW)
        {
            delay(200);
            return PB_CANCEL;
        }

        update_time();
    }
}

//
// Menu Functions
//
void go_to_menu()
{
    while (digitalRead(PB_CANCEL) == HIGH)
    {
        display.clearDisplay();
        print_line(modes[current_mode], 0, 0, 2);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP)
        {
            current_mode += 1;
            current_mode = current_mode % max_modes;
        }

        else if (pressed == PB_DOWN)
        {
            delay(200);
            current_mode -= 1;
            if (current_mode < 0)
            {
                current_mode = max_modes - 1;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200);
            // Serial.println(current_mode);
            run_mode(current_mode);
        }

        else if (pressed == PB_CANCEL)
        {
            delay(200);
            break;
        }
    }
}

void run_mode(int mode)
{
    if (mode == 0)
    {
        set_time_zone();
    }
    else if (mode == 1 || mode == 2)
    {
        set_alarm(mode - 1);
    }
    else if (mode == 3)
    {
        if (alarms_enabled == false)
        {
            alarms_enabled = true;
            display.clearDisplay();
            print_line("Alarms Enabled!", 0, 0, 2);
            delay(1000);
        }
        else if (alarms_enabled == true)
        {
            alarms_enabled = false;
            display.clearDisplay();
            print_line("Alarms Disabled", 0, 0, 2);
            delay(1000);
        }
    }
    else if (mode == 4)
    {
        view_active_alarms();
    }
    else if (mode == 5)
    {
        delete_alarm();
    }
}

//
// Time Functions
//
void print_time_now(void)
{
    display.clearDisplay();
    print_line_centered(String(hours) + ":" + String(minutes) + ":" + String(seconds), 0, 2); // Display the time
    print_line_centered(date, 20, 1);                                                         // Display the date
    display.display();
}

void update_time()
{
    struct tm timeinfo;
    getLocalTime(&timeinfo);

    char timeHour[3];
    strftime(timeHour, 3, "%H", &timeinfo);
    hours = atoi(timeHour);

    char timeMinute[3];
    strftime(timeMinute, 3, "%M", &timeinfo);
    minutes = atoi(timeMinute);

    char timeSecond[3];
    strftime(timeSecond, 3, "%S", &timeinfo);
    seconds = atoi(timeSecond);

    char timeDay[3];
    strftime(timeDay, 3, "%d", &timeinfo);
    days = atoi(timeDay);

    char fullDate[11];
    strftime(fullDate, 11, "%d-%m-%Y", &timeinfo);
    date = String(fullDate);
}

void update_time_with_check_alarm(void)
{
    update_time();
    print_time_now();

    if (alarms_enabled == true)
    {
        for (int i = 0; i < n_alarms; i++)
        {
            if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes)
            {
                alarm_triggered[i] = true;
                ring_alarm();
                // alarm_triggered[i] = true;
            }
        }
    }
}

void set_time_zone()
{
    bool time_set = false;

    int utc_temp_hour = UTC_OFFSET_HOURS;
    while (true)
    {
        display.clearDisplay();
        print_line("Enter UTC Hour Offset: " + String(utc_temp_hour), 0, 0, 2);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP)
        {
            delay(200);
            utc_temp_hour += 1;
            if (utc_temp_hour > 14)
            {
                utc_temp_hour = -12;
            }
        }
        else if (pressed == PB_DOWN)
        {
            delay(200);
            utc_temp_hour -= 1;
            if (utc_temp_hour < -12)
            {
                utc_temp_hour = 14;
            }
        }
        else if (pressed == PB_OK)
        {
            delay(200);
            UTC_OFFSET_HOURS = utc_temp_hour;
            time_set = true;
            break;
        }
        else if (pressed == PB_CANCEL)
        {
            delay(200);
            break;
        }
    }

    int utc_temp_minute = UTC_OFFSET_MINUTES;
    while (true)
    {
        display.clearDisplay();
        print_line("Enter UTC Minute Offset: " + String(utc_temp_minute), 0, 0, 2);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP)
        {
            delay(200);
            utc_temp_minute += 1;
            utc_temp_minute = utc_temp_minute % 60;
        }
        else if (pressed == PB_DOWN)
        {
            delay(200);
            utc_temp_minute -= 1;
            if (utc_temp_minute < 0)
            {
                utc_temp_minute = 59;
            }
        }
        else if (pressed == PB_OK)
        {
            delay(200);
            UTC_OFFSET_MINUTES = utc_temp_minute;
            time_set = true;
            break;
        }
        else if (pressed == PB_CANCEL)
        {
            delay(200);
            break;
        }
    }

    UTC_OFFSET = UTC_OFFSET_HOURS * 3600 + UTC_OFFSET_MINUTES * 60;
    configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

    display.clearDisplay();
    if (time_set == true)
    {
        print_line_centered("Time Zone is set to", 20, 1);
        print_line_centered("UTC: " + String(UTC_OFFSET_HOURS) + ":" + String(UTC_OFFSET_MINUTES), 40, 1);
        display.display();
        delay(2000);
    }
}

void set_time()
{
    int temp_hour = hours;
    while (true)
    {
        display.clearDisplay();
        print_line("Enter Hour: " + String(temp_hour), 0, 0, 2);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP)
        {
            delay(200);
            temp_hour += 1;
            temp_hour = temp_hour % 24;
        }

        else if (pressed == PB_DOWN)
        {
            delay(200);
            temp_hour -= 1;
            if (temp_hour < 0)
            {
                temp_hour = 23;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200);
            hours = temp_hour;
            break;
        }

        else if (pressed == PB_CANCEL)
        {
            delay(200);
            break;
        }
    }

    int temp_minute = minutes;
    while (true)
    {
        display.clearDisplay();
        print_line("Enter minute: " + String(temp_minute), 0, 0, 2);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP)
        {
            delay(200);
            temp_minute += 1;
            temp_minute = temp_minute % 60;
        }

        else if (pressed == PB_DOWN)
        {
            delay(200);
            temp_minute -= 1;
            if (temp_minute < 0)
            {
                temp_minute = 59;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200);
            minutes = temp_minute;
            break;
        }

        else if (pressed == PB_CANCEL)
        {
            delay(200);
            break;
        }
    }

    display.clearDisplay();
    print_line("Time is Set", 0, 0, 2);
    delay(1000);
}

//
// Alarm Functions
//
void set_alarm(int alarm)
{
    int temp_hour = alarm_hours[alarm];
    while (true)
    {
        display.clearDisplay();
        print_line("Enter Hour: " + String(temp_hour), 0, 0, 2);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP)
        {
            delay(200);
            temp_hour += 1;
            temp_hour = temp_hour % 24;
        }

        else if (pressed == PB_DOWN)
        {
            delay(200);
            temp_hour -= 1;
            if (temp_hour < 0)
            {
                temp_hour = 23;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200);
            alarm_hours[alarm] = temp_hour;
            break;
        }

        else if (pressed == PB_CANCEL)
        {
            delay(200);
            break;
        }
    }
    int temp_minute = alarm_minutes[alarm];
    while (true)
    {
        display.clearDisplay();
        print_line("Enter minute: " + String(temp_minute), 0, 0, 2);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP)
        {
            delay(200);
            temp_minute += 1;
            temp_minute = temp_minute % 60;
        }

        else if (pressed == PB_DOWN)
        {
            delay(200);
            temp_minute -= 1;
            if (temp_minute < 0)
            {
                temp_minute = 59;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200);
            alarm_minutes[alarm] = temp_minute;
            break;
        }

        else if (pressed == PB_CANCEL)
        {
            delay(200);
            break;
        }
    }
    display.clearDisplay();
    print_line("Alarm is Set", 0, 0, 2);
    print_line(String(alarm_hours[alarm]) + ":" + String(alarm_minutes[alarm]), 0, 40, 2);
    delay(1000);
}

void view_active_alarms()
{
    display.clearDisplay();
    print_line("Viewing Alarms", 0, 0, 1);
    delay(1000);

    for (int i = 0; i < n_alarms; i++)
    {
        if (alarms_enabled == true && alarm_triggered[i] == false)
        {
            print_line("Alarm " + String(i + 1) + " will Ring at " + String(alarm_hours[i]) + " : " + String(alarm_minutes[i]), 0, 20 + (i * 20), 1);
        }
        else if (alarm_triggered[i] == true)
        {
            print_line("Alarm " + String(i + 1) + " is already Triggered", 0, 20 + (i * 20), 1);
        }
        else if (alarms_enabled == false && alarm_triggered[i] == false)
        {
            print_line("Alarm " + String(i + 1) + " is Disabled", 0, 20 + (i * 20), 1);
        }
    }
    delay(2000);
}

void delete_alarm()
{
    int alarm_to_delete = 0; // This will range between 0 and n_alarms-1

    while (true)
    {
        display.clearDisplay();
        print_line("Enter Alarm to Delete: " + String(alarm_to_delete + 1), 0, 0, 2);

        int pressed = wait_for_button_press();

        if (pressed == PB_UP)
        {
            delay(200);
            alarm_to_delete += 1;
            alarm_to_delete = alarm_to_delete % n_alarms;
        }

        else if (pressed == PB_DOWN)
        {
            delay(200);
            alarm_to_delete -= 1;
            if (alarm_to_delete < 0)
            {
                alarm_to_delete = n_alarms - 1;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200);
            alarm_triggered[alarm_to_delete] = true;
            break;
        }

        else if (pressed == PB_CANCEL)
        {
            delay(200);
            break;
        }
    }

    display.clearDisplay();
    print_line("Alarm " + String(alarm_to_delete + 1) + " is Deleted", 0, 0, 2);
    delay(2000);
}

void ring_alarm(void)
{
    display.clearDisplay();
    print_line("MEDICINE TIME!", 0, 0, 2);
    digitalWrite(LED_1, HIGH);

    bool break_happened = false;

    // Ring the buzzer
    while (break_happened == false && digitalRead(PB_CANCEL) == HIGH)
    {
        for (int i = 0; i < n_notes; i++)
        {
            if (digitalRead(PB_CANCEL) == LOW)
            {
                delay(200);
                break_happened = true;
                // alarm_triggered[i] = true; // Reset the triggered flag for this alarm
                break;
            }
            else if (digitalRead(PB_SNOOZE) == LOW)
            {
                delay(200);
                update_time();

                for (int i = 0; i < n_alarms; i++)
                {
                    if (alarm_hours[i] == hours && alarm_minutes[i] == minutes)
                    {
                        alarm_minutes[i] += 1;
                        if (alarm_minutes[i] >= 60)
                        {
                            alarm_hours[i] += 1;
                            alarm_minutes[i] = alarm_minutes[i] % 60;
                            alarm_hours[i] = alarm_hours[i] % 24;
                        }
                        alarm_triggered[i] = false;
                    }
                }
                display.clearDisplay();
                print_line("Snoozed for 1 minutes", 0, 0, 2);
                delay(1000);

                break_happened = true;
                break;
            }

            tone(BUZZER, notes[i]);
            delay(500);
            noTone(BUZZER);
            delay(2);
        }
    }

    digitalWrite(LED_1, LOW);
    display.clearDisplay();
}

//
//  Temperature and Humidity Functions
//
TempHum check_temp()
{
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    display.clearDisplay();
    print_time_now();
    if (data.temperature > 32)
    {
        // display.clearDisplay();
        print_line_centered("TEMP HIGH", 40, 1);
        print_line_centered(String(data.temperature, 1) + " C", 50, 1);
        display.display();
        digitalWrite(LED_2, HIGH);
        delay(500);
        digitalWrite(LED_2, LOW);
    }
    else if (data.temperature < 24)
    {
        // display.clearDisplay();
        print_line_centered("TEMP LOW", 40, 1);
        print_line_centered(String(data.temperature, 1) + " C", 50, 1);
        display.display();
        digitalWrite(LED_2, HIGH);
        delay(500);
        digitalWrite(LED_2, LOW);
    }
    else if (data.humidity > 80)
    {
        // display.clearDisplay();
        print_line_centered("HUMIDITY HIGH", 40, 1);
        print_line_centered(String(data.humidity, 1) + " %", 50, 1);
        display.display();
        digitalWrite(LED_2, HIGH);
        delay(500);
        digitalWrite(LED_2, LOW);
    }
    else if (data.humidity < 65)
    {
        // display.clearDisplay();
        print_line_centered("HUMIDITY LOW", 40, 1);
        print_line_centered(String(data.humidity, 1) + " %", 50, 1);
        display.display();
        digitalWrite(LED_2, HIGH);
        delay(500);
        digitalWrite(LED_2, LOW);
    }

    TempHum result;
    result.temperature = data.temperature;
    result.humidity = data.humidity;

    // Update latestTemperature
    latestTemperature = data.temperature;

    return result;
}

//
// LDR Functions
//
void sampleLDR()
{
    int raw = analogRead(LDR);                // 0 - 4095
    float norm = 1.0 - ((float)raw / 4095.0); // normalize to 0 - 1
    lightSum += norm;
    sampleCount++;
    // Update latestIntensity with the new average
    latestIntensity = (sampleCount > 0) ? (lightSum / sampleCount) : 0.0;
}

//
// Servo Functions
//
void computeServoAngle(float intensity, float temperature)
{

    float T = temperature;
    if (isnan(T))
    {
        Serial.println("Temperature read error");
        return;
    }

    float lnRatio = log((float)ts / (float)tu); // natural log
    float theta = thetaOfset + (180.0 - thetaOfset) * intensity * gammaValue * lnRatio * (T / Tmed);
    theta = constrain(theta, 0, 180);

    servo.write((int)theta);
    Serial.print("Servo angle: ");
    Serial.println(theta);
}

//
// MQTT Functions
//
void connectToMQTT()
{
    while (!mqttclient.connected())
    {
        Serial.print("Attempting MQTT connection...");
        String clientId = "ESP32Client-" + String(random(0, 1000));
        if (mqttclient.connect(clientId.c_str()))
        {
            Serial.println("connected");
            mqttclient.subscribe("/Medibox/ts");
            mqttclient.subscribe("/Medibox/tu");
            mqttclient.subscribe("/Medibox/gammaValue");
            mqttclient.subscribe("/Medibox/thetaOfset");
            mqttclient.subscribe("/Medibox/Tmed");

            // Publish initial values as a single JSON object to dashboard
            JsonDocument doc;
            doc["ts"] = ts;
            doc["tu"] = tu;
            doc["gammaValue"] = gammaValue;
            doc["thetaOfset"] = thetaOfset;
            doc["Tmed"] = Tmed;
            char jsonString[200];
            serializeJson(doc, jsonString);
            Serial.println(jsonString);
            mqttclient.publish("/Medibox/params", jsonString);
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(mqttclient.state());
            delay(5000);
        }
    }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message received on topic: ");
    Serial.println(topic);

    String msg;
    for (unsigned int i = 0; i < length; i++)
    {
        msg += (char)payload[i];
    }
    Serial.print("Payload: ");
    Serial.println(msg);

    float val = msg.toFloat();

    bool servoParamChanged = false;

    if (strcmp(topic, "/Medibox/ts") == 0)
    {
        ts = max(1, (int)val); // Prevent 0
        Serial.print("ts updated: ");
        Serial.println(ts);
        servoParamChanged = true;
    }
    else if (strcmp(topic, "/Medibox/tu") == 0)
    {
        tu = max(5, (int)val); // Prevent too low
        Serial.print("tu updated: ");
        Serial.println(tu);
        servoParamChanged = true;
    }
    else if (strcmp(topic, "/Medibox/thetaOfset") == 0)
    {
        thetaOfset = constrain(val, 0, 120);
        servoParamChanged = true;
    }
    else if (strcmp(topic, "/Medibox/gammaValue") == 0)
    {
        gammaValue = constrain(val, 0.0, 1.0);
        servoParamChanged = true;
    }
    else if (strcmp(topic, "/Medibox/Tmed") == 0)
    {
        Tmed = constrain(val, 10.0, 40.0);
        servoParamChanged = true;
    }

    // If any servo parameter changed, recompute and set servo angle
    if (servoParamChanged) {
        // Use the latest available LDR average (or 0 if no samples yet)
        float avgIntensity = (sampleCount > 0) ? (lightSum / sampleCount) : 0.0;
        // Use the latest temperature reading
        TempHum th = check_temp();
        computeServoAngle(avgIntensity, th.temperature);
    }
}

void publishData(float temperature, float humidity)
{
    if (sampleCount == 0)
    {
        Serial.println("Skipping send: no LDR samples collected.");
        return;
    }

    float avgIntensity = lightSum / sampleCount;

    lightSum = 0;
    sampleCount = 0;

    JsonDocument doc;

    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["intensity"] = avgIntensity;

    char jsonString[200];
    serializeJson(doc, jsonString);
    Serial.println(jsonString);
    mqttclient.publish("medibox/sensor_data", jsonString);
}
