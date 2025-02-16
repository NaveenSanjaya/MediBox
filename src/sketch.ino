// Include libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Define the OLED display parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define BUZZER 5
#define LED_1 15
#define PB_CANCEL 34
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 35

int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

unsigned long timeNow = 0;
unsigned long timeLast = 0;

bool alarm_enabled = true;
int n_alarms = 2;
int alarm_hours[] = {0, 1};
int alarm_minutes[] = {1, 10};
bool alarm_triggered[] = {false, false};

int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

int current_mode = 0;
int max_mode = 4;
String modes[] = {"1 - Set Time", "2 - Set Alarm 1", "3 - Set Alarm 2", "4 - Disable Alarms"};

// Declear the OLED display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup()
{
    pinMode(BUZZER, OUTPUT);
    pinMode(LED_1, OUTPUT);
    pinMode(PB_CANCEL, INPUT);
    pinMode(PB_OK, INPUT);
    pinMode(PB_UP, INPUT);
    pinMode(PB_DOWN, INPUT);

    // put your setup code here, to run once:
    Serial.begin(9600);

    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ;
    }

    // Show initial display buffer contents on the screen
    display.display();
    delay(2000); // Pause for 2 seconds

    // Clear the buffer
    display.clearDisplay();

    print_line("Welcome to Medibox!", 10, 20, 2);
    display.clearDisplay();
}

void loop()
{
    update_time_with_check_alarm();

    if (digitalRead(PB_OK) == LOW)
    {
        delay(200); // Debounce
        go_to_menu();
    }
}

void print_line(String text, int column, int row, int text_size)
{

    display.setTextSize(text_size);      // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(column, row);      // Start at top-left corner
    display.println(text);

    display.display();
}

void print_time_now(void)
{
    display.clearDisplay();

    print_line(String(days), 0, 0, 2);
    print_line(":", 20, 0, 2);
    print_line(String(hours), 30, 0, 2);
    print_line(":", 50, 0, 2);
    print_line(String(minutes), 60, 0, 2);
    print_line(":", 80, 0, 2);
    print_line(String(seconds), 80, 0, 2);
}

void update_time()
{

    timeNow = millis() / 1000;    // Get the current time in seconds
    seconds = timeNow - timeLast; // Calculate the time difference

    if (seconds >= 60)
    {
        minutes += 1;
        timeLast += 60;
    }

    if (minutes >= 60)
    {
        hours += 1;
        minutes = 0;
    }

    if (hours >= 24)
    {
        days += 1;
        hours = 0;
    }
}

void ring_alarm()
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
                delay(200); // Debounce
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

void update_time_with_check_alarm()
{
    update_time();
    print_time_now();

    if (alarm_enabled == true)
    {
        for (int i = 0; i < n_alarms; i++)
        {
            if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes)
            {
                ring_alarm();
                alarm_triggered[i] = true;
            }
        }
    }
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

        else if (digitalRead(PB_DOWN) == LOW)
        {
            delay(200); // Debounce
            return PB_DOWN;
        }

        else if (digitalRead(PB_OK) == LOW)
        {
            delay(200); // Debounce
            return PB_OK;
        }

        else if (digitalRead(PB_CANCEL) == LOW)
        {
            delay(200); // Debounce
            return PB_CANCEL;
        }

        update_time();
    }
}

void go_to_menu()
{
    while (digitalRead(PB_CANCEL) == HIGH)
    {
        display.clearDisplay();
        print_line(modes[current_mode], 0, 0, 2);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP)
        {
            delay(200); // Debounce
            current_mode += 1;
            current_mode = current_mode % max_mode;
        }

        else if (pressed == PB_DOWN)
        {
            delay(200); // Debounce
            current_mode -= 1;
            if (current_mode < 0)
            {
                current_mode = max_mode - 1;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200); // Debounce
            run_mode(current_mode);
        }
        else if (pressed == PB_CANCEL)
        {
            delay(200); // Debounce
            break;
        }
    }
}

void set_time()
{
    int temp_hour = hours;
    while (true)
    {
        display.clearDisplay();
        print_line("Enter hour" + String(temp_hour), 0, 0, 2);
        int pressed = wait_for_button_press();
        if (pressed == PB_UP)
        {
            delay(200); // Debounce
            temp_hour += 1;
            temp_hour = temp_hour % 24;
        }

        else if (pressed == PB_DOWN)
        {
            delay(200); // Debounce
            temp_hour -= 1;
            if (temp_hour < 0)
            {
                temp_hour = 23;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200); // Debounce
            hours = temp_hour;
            break;
        }
        else if (pressed == PB_CANCEL)
        {
            delay(200); // Debounce
            break;
        }
    }

    int temp_minute = minutes;
    while (true)
    {
        display.clearDisplay();
        print_line("Enter minute" + String(temp_minute), 0, 0, 2);
        int pressed = wait_for_button_press();
        if (pressed == PB_UP)
        {
            delay(200); // Debounce
            temp_minute += 1;
            temp_minute = temp_minute % 60;
        }

        else if (pressed == PB_DOWN)
        {
            delay(200); // Debounce
            temp_minute -= 1;
            if (temp_minute < 0)
            {
                temp_minute = 59;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200); // Debounce
            minutes = temp_minute;
            break;
        }
        else if (pressed == PB_CANCEL)
        {
            delay(200); // Debounce
            break;
        }
    }

    display.clearDisplay();
    print_line("Time is set", 0, 0, 2);
    delay(1000);
}

void set_alarm(int alarm)
{
    int temp_hour = alarm_hours[alarm];
    while (true)
    {
        display.clearDisplay();
        print_line("Enter hour" + String(temp_hour), 0, 0, 2);
        int pressed = wait_for_button_press();
        if (pressed == PB_UP)
        {
            delay(200); // Debounce
            temp_hour += 1;
            temp_hour = temp_hour % 24;
        }

        else if (pressed == PB_DOWN)
        {
            delay(200); // Debounce
            temp_hour -= 1;
            if (temp_hour < 0)
            {
                temp_hour = 23;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200); // Debounce
            alarm_hours[alarm] = temp_hour;
            break;
        }
        else if (pressed == PB_CANCEL)
        {
            delay(200); // Debounce
            break;
        }
    }

    int temp_minute = alarm_minutes[alarm];
    while (true)
    {
        display.clearDisplay();
        print_line("Enter minute" + String(temp_minute), 0, 0, 2);
        int pressed = wait_for_button_press();
        if (pressed == PB_UP)
        {
            delay(200); // Debounce
            temp_minute += 1;
            temp_minute = temp_minute % 60;
        }

        else if (pressed == PB_DOWN)
        {
            delay(200); // Debounce
            temp_minute -= 1;
            if (temp_minute < 0)
            {
                temp_minute = 59;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200); // Debounce
            alarm_minutes[alarm] = temp_minute;
            break;
        }
        else if (pressed == PB_CANCEL)
        {
            delay(200); // Debounce
            break;
        }
    }

    display.clearDisplay();
    print_line("Alarm is set", 0, 0, 2);
    delay(1000);
}

void run_mode(int mode)
{
    if (mode == 0)
    {
        set_time();
    }

    else if (mode == 1 || mode == 2)
    {
        set_alarm(mode - 1);
    }

    else if (mode == 3)
    {
        alarm_enabled = false;
    }
}