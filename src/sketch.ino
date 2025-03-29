// include libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>

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
#define PB_UP 33
#define PB_SNOOZE 25
#define DHTPIN 12

// Declare Objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

// Function Declarations
void print_line(String text, int column, int row, int text_size);
void print_time_now(void);
void update_time(void);
void ring_alarm(void);
void update_time_with_check_alarm(void);
void go_to_menu();
void set_time();
void set_time_zone();
void set_alarm(int alarm);
void run_mode(int mode);
void check_temp();
void view_active_alarms();
void delete_alarm();

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
int alarm_hours[] = {0, 1};
int alarm_minutes[] = {1, 10};
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

int current_mode = 0;
int max_modes = 6;
String modes[] = {"1- \nSet Time \nZone", "2- \nSet Alarm 1", "3- \nSet Alarm 2", "4- \nEnable/\nDisable \nAlarm", "5- \nView \nActive \nAlarms", "6- \nDelete \nAlarm"};

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

    dhtSensor.setup(DHTPIN, DHTesp::DHT22);

    Serial.begin(9600);

    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ;
    }

    // show the display buffer on the screen. you MUST call display() after
    // drawing commands to make them visible on screen!
    display.clearDisplay();

    WiFi.begin("Wokwi-GUEST", "", 6);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(250);
        display.clearDisplay();
        print_line("Connecting to WiFi", 0, 0, 2);
    }

    display.clearDisplay();
    print_line("Connected to WiFi", 0, 0, 2);

    configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

    // clear the buffer
    display.clearDisplay();

    print_line_centered("Welcome", 10, 2);
    print_line_centered("to Medibox!", 30, 1.5);
    display.display();
    delay(500);
    display.clearDisplay();
}

void loop()
{
    // put your main code here, to run repeatedly:
    update_time_with_check_alarm();

    if (digitalRead(PB_OK) == LOW)
    {
        delay(200);
        go_to_menu();
    }
    check_temp();
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
        print_line(modes[current_mode], 0, 0, 1);

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
    print_line_centered(String(hours) + ":" + String(minutes) + ":" + String(seconds), 20, 2); // Display the time
    print_line_centered(date, 40, 1); // Display the date
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
                ring_alarm(i);
                alarm_triggered[i] = true;
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
        print_line("Time Zone is Set", 0, 0, 2);
        delay(1000);
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
    print_line(String(alarm_hours[alarm]) + ":" + String(alarm_minutes[alarm]), 0, 20, 2);
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
        print_line("Enter Specific Alarm to Delete: " + String(alarm_to_delete + 1), 0, 0, 2);

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

void ring_alarm(int alarm_index)
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
                break;
            }
            else if (digitalRead(PB_SNOOZE) == LOW)
            {
                delay(200); // Debounce

                // Add 5 minutes to the snoozed alarm
                alarm_minutes[alarm_index] += 5;
                if (alarm_minutes[alarm_index] >= 60)
                {
                    alarm_minutes[alarm_index] -= 60;
                    alarm_hours[alarm_index] += 1;
                    if (alarm_hours[alarm_index] >= 24)
                    {
                        alarm_hours[alarm_index] = 0; // Wrap around to 0 if it exceeds 23
                    }
                }

                // Display snooze message
                display.clearDisplay();
                print_line("Snoozed for 5 mins", 0, 0, 2);
                delay(1000);
                display.clearDisplay();

                break_happened = true; // Exit the alarm loop after snoozing
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
void check_temp()
{
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    if (data.temperature > 32)
    {
        display.clearDisplay();
        print_line("TEMP HIGH", 0, 50, 1);
        print_line(String(data.temperature, 1) + " C", 0, 30, 1);
        digitalWrite(LED_2, HIGH);
        delay(1000);
        digitalWrite(LED_2, LOW);
    }
    else if (data.temperature < 24)
    {
        display.clearDisplay();
        print_line("TEMP LOW", 0, 50, 1);
        print_line(String(data.temperature, 1) + " C", 0, 30, 1);
        digitalWrite(LED_2, HIGH);
        delay(1000);
        digitalWrite(LED_2, LOW);
    }
    else if (data.humidity > 80)
    {
        display.clearDisplay();
        print_line("HUMIDITY HIGH", 0, 50, 1);
        print_line(String(data.humidity, 1) + " %", 0, 30, 1);
        digitalWrite(LED_2, HIGH);
        delay(1000);
        digitalWrite(LED_2, LOW);
    }
    else if (data.humidity < 65)
    {
        display.clearDisplay();
        print_line("HUMIDITY LOW", 0, 50, 1);
        print_line(String(data.humidity, 1) + " %", 0, 30, 1);
        digitalWrite(LED_2, HIGH);
        delay(1000);
        digitalWrite(LED_2, LOW);
    }
}
