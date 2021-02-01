// A tick-based water meter
#include "EEPROM.h"
#include "FixedOled.h"

// channel number
#define ZUNO_CHANNEL_NUMBER_COLD   1

#define EEPROM_ADDR              0x2200
#define EEPROM_RING_SIZE_ENTRIES 10000

// The meter is connected to PIN 18
#define PIN_TICK 18

// On-demand display
OLED oled;

// Tick value
unsigned long tick_count = 0;

// EEPROM position
unsigned long cur_eeprom_offset = 0;

// Current tick start time (high reading start)
bool tick_started = false;
unsigned long tick_start_time = 0;

ZUNO_SETUP_CHANNELS(ZUNO_METER(ZUNO_METER_TYPE_WATER, METER_RESET_ENABLE, 
    ZUNO_METER_WATER_SCALE_GALLONS, METER_SIZE_FOUR_BYTES, METER_PRECISION_ONE_DECIMAL, getterCold, resetterCold));

void write_to_eeprom() {
  cur_eeprom_offset += 1
  if (cur_eeprom_offset == EEPROM_RING_SIZE_ENTRIES) {
    cur_eeprom_offset = 0;
  }

  unsigned int sz = sizeof(tick_count);
  EEPROM.put(EEPROM_ADDR+(cur_eeprom_offset*3+0)*sz, &tick_count, sz);
  EEPROM.put(EEPROM_ADDR+(cur_eeprom_offset*3+1)*sz, &tick_count, sz);
  EEPROM.put(EEPROM_ADDR+(cur_eeprom_offset*3+2)*sz, &tick_count, sz);
}

unsigned long read_entry(unsigned int idx) {
  unsigned int sz = sizeof(tick_count);
  unsigned long v1, v2, v3;
  EEPROM.put(EEPROM_ADDR+(cur_eeprom_offset*3+0)*sz, &v1, sz);
  EEPROM.put(EEPROM_ADDR+(cur_eeprom_offset*3+1)*sz, &v2, sz);
  EEPROM.put(EEPROM_ADDR+(cur_eeprom_offset*3+2)*sz, &v3, sz);
  if (v1 == v2 && v2 == v3) {
    return v1;
  }
  if (v1 == v2) {
    return v1;
  }
  if (v2 == v3) {
    return v2;
  }
  if (v1 == v3) {
    return v1;
  }
  return 0;
}

void read_from_eeprom() {
  unsigned long max_val = 0;
  for(unsigned int i = 0; i < EEPROM_RING_SIZE_ENTRIES; i++) {
     unsigned long cur = read_entry(i);
     if (cur > max_val) {
        max_val = cur;
     }
  }
  tick_count = max_val;
}

void clear_eeprom() {
  unsigned char zero = 0;
  for(unsigned int i = 0; i < EEPROM_RING_SIZE_ENTRIES*3*sizeof(unsigned long); i++) {
     EEPROM.put(EEPROM_ADDR + i, &zero, 1);
  }
}

bool isGreaterThan(unsigned long t1, unsigned long t2, unsigned long diff) {
  return false;
}

void initOled() {
  // Prepare the I2C pins
  pinMode(7, OUTPUT);
  digitalWrite(7, LOW);
  pinMode(8, OUTPUT);
  digitalWrite(8, LOW);

  // Reset the OLED display
  int oledResetPin = 11;
  pinMode(oledResetPin, OUTPUT);
  digitalWrite(oledResetPin, HIGH);
  delay(10);                  // VDD goes high at start, pause for 10 ms
  digitalWrite(oledResetPin, LOW);  // Bring reset low
  delay(10);                  // Wait 10 ms
  digitalWrite(oledResetPin, HIGH); // Bring out of reset
  delay(100);
  // Start the OLED
  oled.begin();
  delay(50);
  oled.setFont(SmallFont);
  oled.clrscr();
  oled.on();
}

void setup() {
  initOled();
  // Dry contacts of meters connect to these pins
  zunoExtIntMode(ZUNO_EXT_INT1, RISING);
  pinMode(PIN_TICK, INPUT_PULLUP);

  // Get last meter values from EEPROM
  read_from_eeprom();
}

void printMeter() {
  //oled.clrscr();
  oled.gotoXY(0,1);
//  oled.print("Gal: ");
//  oled.print(my_meter_data.ticks_cold);
  oled.print(123456789);
}

void loop() {
/*
  if (last_reported_data.ticks_cold != my_meter_data.ticks_cold) {
    last_reported_data.ticks_cold = my_meter_data.ticks_cold;
    data_updated = true;
    printMeter();
  }
    
  // To save EEPROM from a lot of r/w operation 
  // write it once in EEPROM_UPDATE_INTERVAL if data was updated
  if (data_updated && (millis() - last_update_millis) > EEPROM_UPDATE_INTERVAL) {
      zunoSendReport(ZUNO_CHANNEL_NUMBER_COLD);
      update_meter_data();
      data_updated = false;
      last_update_millis =  millis();
  }
  delay(1000);
*/
  delay(20);
}

void resetterCold(byte v) {
  tick_count = 0;
  clear_eeprom();
  write_to_eeprom();
}

unsigned long getterCold(void) {
  return tick_count;
}
