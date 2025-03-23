// Add this to your global variables section
int time_zone = 0; // Default UTC offset

// Update your configTime call in setup() to use the variable
// configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
// Change to:
configTime(time_zone * 3600, 0, NTP_SERVER);

// Add this function for setting time zone
void set_time_zone() {
  int temp_timezone = time_zone;
  
  display.clearDisplay();
  print_line("Set Time Zone", 0, 0, 2);
  print_line("Current: UTC" + String(temp_timezone >= 0 ? "+" : "") + String(temp_timezone), 0, 20, 1);
  delay(1000);
  
  while (true) {
    display.clearDisplay();
    print_line("Time Zone: UTC" + String(temp_timezone >= 0 ? "+" : "") + String(temp_timezone), 0, 0, 2);
    
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200); // Debounce
      temp_timezone += 1;
      if (temp_timezone > 12) {
        temp_timezone = 12;
      }
    }
    else if (pressed == PB_DOWN) {
      delay(200); // Debounce
      temp_timezone -= 1;
      if (temp_timezone < -12) {
        temp_timezone = -12;
      }
    }
    else if (pressed == PB_OK) {
      delay(200); // Debounce
      time_zone = temp_timezone;
      configTime(time_zone * 3600, 0, NTP_SERVER); // Update time with new time zone
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200); // Debounce
      break;
    }
  }
  
  display.clearDisplay();
  print_line("Time Zone Set", 0, 0, 2);
  print_line("UTC" + String(time_zone >= 0 ? "+" : "") + String(time_zone), 0, 20, 2);
  delay(1000);
}