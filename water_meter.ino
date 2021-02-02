// A tick-based water meter
#include "EEPROM.h"
#include "FixedOled.h"

// channel number
#define ZUNO_CHANNEL_NUMBER 1

#define EEPROM_ADDR              0x2200
#define EEPROM_RING_SIZE_ENTRIES 10000

#define SLEEP_TIME_MS 20
#define NUM_DEBOUNCE 3

// The meter is connected to PIN 18
#define PIN_TICK 21

// A couple of utility pins
#define RST_PIN 22
#define OLED_READOUT_PIN 20
#define OLED_COUNTS_TO_SHINE 10000 // 200 seconds

//ZUNO_DISABLE(SERVICE_LEDS); // Disable Service LEDs to save battry and remove unwanted blinks

// On-demand display
OLED oled;

// Tick value
unsigned long tick_count = 0;

// EEPROM position
unsigned long cur_eeprom_offset = 0;

// Current tick start time (high reading start)
bool tick_started = false;
unsigned int debounce_count = 0;

// OLED time to shine
bool oled_is_on = false;
unsigned int oled_counts_left = 0;

// Unsolicited report
unsigned long last_report_millis = 0;

ZUNO_SETUP_CHANNELS(ZUNO_METER(ZUNO_METER_TYPE_WATER, METER_RESET_DISABLE, 
    ZUNO_METER_WATER_SCALE_GALLONS, METER_SIZE_FOUR_BYTES, METER_PRECISION_ONE_DECIMAL, getter, NULL));

ZUNO_SETUP_CFGPARAMETER_HANDLER(tick_value_push);


void write_to_eeprom() {
  unsigned long buf[3];
  buf[0] = buf[1] = buf[2] = tick_count;
  EEPROM.put(EEPROM_ADDR + cur_eeprom_offset*sizeof(buf), buf, sizeof(buf)); 
 
  cur_eeprom_offset += 1;
  if (cur_eeprom_offset == EEPROM_RING_SIZE_ENTRIES) {
    cur_eeprom_offset = 0;
  }
}

unsigned long read_entry(unsigned long idx) {
  unsigned long buf[3];
  EEPROM.get(EEPROM_ADDR + idx*sizeof(buf), buf, sizeof(buf)); 
  unsigned long v1 = buf[0], v2 = buf[1], v3 = buf[2];
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

void init_value_from_eeprom() {
  unsigned long max_val = 0;
  unsigned long last_reported = 0;

  for(unsigned long i = 0; i < EEPROM_RING_SIZE_ENTRIES; i++) {
     unsigned long cur = read_entry(i);
     if (cur > max_val) {
        max_val = cur;
	cur_eeprom_offset = i;
     }
  
     unsigned long cur_pct = (i*100/EEPROM_RING_SIZE_ENTRIES);
     if (cur_pct >= last_reported+5) {
     	oled.gotoXY(0, 3);
	oled.print(cur_pct);
	oled.println("%");
	last_reported = cur_pct;
     }
  }
  tick_count = max_val;
}

void clear_eeprom() {
  unsigned long last_reported = 0;
  unsigned long buf[3];
  buf[0] = buf[1] = buf[2] = 0;
  
  oled.clrscr();
  oled.gotoXY(0,1);
  oled.println("CLEARING EEPROM");
  for(unsigned long i = 0; i < EEPROM_RING_SIZE_ENTRIES; i++) {
     EEPROM.put(EEPROM_ADDR + i*sizeof(buf), buf, sizeof(buf));
     
     unsigned long cur_pct = (i*100/EEPROM_RING_SIZE_ENTRIES);
     if (cur_pct >= last_reported+5) {
     	oled.gotoXY(0, 3);
	oled.print(cur_pct);
	oled.println("%");
	last_reported = cur_pct;
     }
  }
  tick_count = cur_eeprom_offset = 0;
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
  oled.gotoXY(0,1);
}

void setup() {
  initOled();
  Serial.begin();
  
  // Check for reset
  pinMode(RST_PIN, INPUT_PULLUP);
  if (!digitalRead(RST_PIN)) {
    clear_eeprom();
    zunoReboot();
  }

  oled.println("Loading...");
  
  // Dry contacts of meters connect to this pin
  pinMode(PIN_TICK, INPUT_PULLUP);

  // Utility pin
  pinMode(OLED_READOUT_PIN, INPUT_PULLUP);

  // Get last meter values from EEPROM
  init_value_from_eeprom();
  enable_oled_readout();
}

void enable_oled_readout() {
  if (!oled_is_on) {
    oled_is_on = true;
    initOled();
    printMeter();
  }
  oled_counts_left = OLED_COUNTS_TO_SHINE;
}

void clear_rest_of_line() {
  while(oled.get_cx() != 0) {
    oled.print(" ");
  }
}

void printMeter() {
  oled.gotoXY(0,1);
  oled.print(" gal: ");
  oled.print(tick_count/10);  
  oled.print(".");
  oled.print(tick_count%10);
  clear_rest_of_line();
  oled.println("");

  double cf3 = double(0.133680556) * (tick_count / 10.0);
  oled.print("cf^3: ");
  oled.print(cf3);
  clear_rest_of_line();
  oled.println("");

  double m3 = double(0.00378541178) * (tick_count / 10.0);
  oled.print(" m^3: ");
  oled.print(m3);
  clear_rest_of_line();
  oled.println("");
}

void loop() {
  if (!digitalRead(OLED_READOUT_PIN)) {
    enable_oled_readout();
  }

  if (digitalRead(PIN_TICK)) {
    if (!tick_started) {
      if (debounce_count == 0) {
        tick_started = true;
        debounce_count = NUM_DEBOUNCE;
      }
    }
  } else {
    if (tick_started) {
      tick_started = false;
      tick_count++;
      write_to_eeprom();

      if (abs(millis() - last_report_millis) > 10000) {
        zunoSendReport(ZUNO_CHANNEL_NUMBER);
	last_report_millis = millis();
      }

      if (oled_is_on) {
        printMeter();
      }
    }
  }
  
  if (debounce_count > 0) {
    debounce_count--;
  }

  if (oled_is_on) {
    oled_counts_left--;
    if (oled_counts_left == 0) {
      oled.off();
      oled_is_on = false;
    }
  }
  
  delay(SLEEP_TIME_MS);
}

unsigned long getter(void) {
  return tick_count;
}

unsigned long uncommitted;
void tick_value_push(byte param, word value) {
  if (param == 65) { 
    // The second user-defined parameter is used to control the upper word of the counter
    uncommitted = ((unsigned long)value) << 16;
  }
  if (param == 66) {
    // The lower user-defined parameter is used to control the lower word of the counter
    enable_oled_readout();
    clear_eeprom();
    tick_count = uncommitted + value;
    write_to_eeprom();
    printMeter();
  }
}
