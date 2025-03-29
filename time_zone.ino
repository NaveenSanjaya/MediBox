void ring_alarm(void){
  
    display.clearDisplay();
    print_line("MEDICINE TIME!", 0, 0, 2);
    digitalWrite(LED_1, HIGH);
  
    bool break_happened = false;
  
    //Ring the buzzer
    while (break_happened == false && digitalRead(PB_CANCEL) == HIGH){
      for (int i=0; i< n_notes; i++){
        if (digitalRead(PB_CANCEL) == LOW){
          delay(200);
          break_happened = true;
          break;
        }
        else if (digitalRead(PB_SNOOZE) == LOW){
          delay(200);
          update_time();
  
          for (int i=0; i<n_alarms; i++){
            if (alarm_hours[i] == hours && alarm_minutes[i] == minutes){
              alarm_minutes[i] += 5;
              if (alarm_minutes[i] >= 60){
                alarm_hours[i] += 1;
                alarm_minutes[i] = alarm_minutes[i] % 60;
                alarm_hours[i] = alarm_hours[i] % 24;
              }
              alarm_triggered[i] = false;
            }
          }
          display.clearDisplay();
          print_line("Snoozed for 5 minutes", 0, 0, 2);
          delay(1000);
          break_happened = true;
          break;
          
        }  
        //Serial.println(notes[i]);
        tone(BUZZER, notes[i]);
        delay(500);
        noTone(BUZZER);
        delay(2);
      }
    }
    digitalWrite(LED_1, LOW);
    display.clearDisplay();
  }