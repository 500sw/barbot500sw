#include <Adafruit_NeoPixel.h> // library to control the LED strip
#include <Servo.h> 
#include <stdarg.h>
void p(char *fmt, ... );

/* -- arduino constants and pins -- */
#define LED_PIN 7
#define BUTTON_PIN 6
#define SERVO_PIN 3

#define TIME_TO_STIR 7000

const uint8_t pumpPins[] = { 8, 9, 10, 11, 12, 4}; 
#define PUMP_PINS_COUNT sizeof(pumpPins)
#define RECIPES_COUNT 6

//vodka, kahlua, triplesec, creme, orange, cranbery
const char* ingredients[PUMP_PINS_COUNT] = {
  "vodka", "kahlua", "triplesec", "creme", "orange", "cranbery" };
const float recipes[RECIPES_COUNT][PUMP_PINS_COUNT] = {
  { 1.0,  0.0, 0.0,  0.0,  0.0,  3.0, }, //Cape Cod
  { 1.0,  0.0, 0.0,  0.0,  3.0,  0.0, }, //Screwdriver
  { 1.0,  0.5, 0.0,  0.0,  0.0,  0.0, }, //Black Russian
  { 1.0,  0.5, 0.0,  1.0,  0.0,  0.0, }, //White Russian
  { 1.25, 0.0, 0.25, 0.0,  0.0,  1.0, }, //Cosmopolitan 
  { 0.75, 0.0, 0.75, 0.75, 0.75, 0.0, }  //Creamsicle Martini
};


Servo servo;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(6, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel drinkLeds = Adafruit_NeoPixel(1, 2, NEO_GRB + NEO_KHZ800);


void setup() {
  Serial.begin(38400);
  pinMode(13, OUTPUT);

  // Lights
  strip.begin();
  strip.show();
  drinkLeds.begin();
  drinkLeds.show();

  // Valves
  for (uint8_t i = 0; i < PUMP_PINS_COUNT; i++) {
    pinMode(pumpPins[i], OUTPUT);
    digitalWrite(pumpPins[i], LOW);
  } 

  // Button
  pinMode(BUTTON_PIN, INPUT);
  digitalWrite(BUTTON_PIN, HIGH);
}

enum system_states {
  STATE_IDLE,
  STATE_COUNTDOWN,
  STATE_DANCING,
  STATE_POURING,
  STATE_DONE
};

#define IDLE_TIME 500
#define COUNTDOWN_TIME 3000

void loop() {
  
  uint8_t last_button = 0;
  uint32_t button_down = 0;
  
  uint32_t first_serial = 0;
  char serial_buffer[32] = "";
  uint8_t serial_i = 0;
  
  uint8_t state = STATE_IDLE;
  uint32_t state_change_time = 0;
  uint8_t chosen_drink = -1;
  int16_t dancing_last_foot = -1;
  uint32_t dancing_last_time = 0;
  
  while (1) {
    uint32_t current = millis();
    uint32_t state_time = (current - state_change_time);
    digitalWrite(13, (current % 500) > 490);


    /* ------------------------- */
    /* ---- button handling ---- */
    /* ------------------------- */
    uint8_t button = !digitalRead(BUTTON_PIN);
    if (button && !last_button) {
      button_down = current;
    } else if (!button && last_button) {
      if ((current - button_down) < 1000) {
        if (state == STATE_IDLE) {
          state = STATE_COUNTDOWN;
          state_change_time = current;
          Serial.println("countdown");
          last_button = button;
        }
      } else { //long press behavior
        Serial.println("long_press");
      }
    }
    if (button && ((current - button_down) > 5000)) {
      Serial.println("clean_mode");
      for (uint8_t pump=0; pump < PUMP_PINS_COUNT; pump++)
        digitalWrite(pumpPins[pump], 1);
    } else if (!button && last_button && ((current - button_down) > 5000)) {
      for (uint8_t pump=0; pump < PUMP_PINS_COUNT; pump++)
        digitalWrite(pumpPins[pump], 0);
    }
    last_button = button;



    /* ------------------------ */
    /* ---- state control ---- */
    /* ------------------------ */
    state_time = (current - state_change_time);
    if ((state == STATE_COUNTDOWN) && (state_time > COUNTDOWN_TIME)) {
      state = STATE_DANCING;
      state_change_time = current;
      Serial.println("start");
    } else if ((state == STATE_DANCING) && (state_time > 25000)) {
      state = STATE_POURING;
      state_change_time = current;
      chosen_drink = 5;
      p("dance timeout");
    } else if (state == STATE_POURING) {
      uint8_t done = pour_and_stir(chosen_drink, state_time);
      if (done) {
        state = STATE_DONE;
        state_change_time = current;
      }
    } else if ((state == STATE_DONE) && ((current - state_change_time) > 4000)) {
      state = STATE_IDLE;
      state_change_time = current;
      Serial.println("done_pouring");
    }


    /* ----------------------------- */
    /* ---- read in serial data ---- */
    /* ----------------------------- */
    while (Serial.available()) {
      if (serial_i == 0) { first_serial = current; }
      serial_buffer[serial_i++] = Serial.read();
      if (serial_i >= sizeof(serial_buffer)) {
        serial_i = 0; //buffer full, reset buffer :(
        Serial.println("reset buffer :(");
      }
    }



    /* ---------------------------- */
    /* ---- command processing ---- */
    /* ---------------------------- */
    if (first_serial && (serial_i > 1) && ((serial_buffer[serial_i] == '\n') || ((current - first_serial) > 100))) {
      char * split = strtok (serial_buffer, " ,");
      
      if (strcmp(split, "foot") == 0) {
        char * number_sp = strtok(NULL, " ,");
        uint8_t foot = (number_sp != NULL)? atoi(number_sp) : -1;
        dancing_last_time = current;
        dancing_last_foot = foot;
      } else if (strcmp(split, "drink") == 0) {
        char * drink_sp = strtok(NULL, " ,");
        int16_t drink = (drink_sp != NULL)? atoi(drink_sp) : -1;
        if (drink >= 0 && drink < 6) {
          state = STATE_POURING;
          state_change_time = current;
          chosen_drink = drink;
          p("pouring drink %d\n", chosen_drink);
        } else {
          state = STATE_IDLE;
          state_change_time = current;
          p("error with drink: '%s'\n", drink_sp);
        }
      } else {
        p("unrecognized command: '%s'\n", split);
      }
      serial_i = first_serial = 0;
    }



    /* ----------------- */
    /* -- led control -- */
    /* ----------------- */
    state_time = (current - state_change_time);
    if (state == STATE_IDLE) {
      for (uint8_t i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, ((state_time / IDLE_TIME) % 6 == i)? Wheel(100) : 0);
      }
    } else if (state == STATE_COUNTDOWN) {
      uint8_t count_down_to = strip.numPixels() + 1;
      uint8_t count_down_at = (state_time / (COUNTDOWN_TIME/count_down_to)) % count_down_to;
      for (uint8_t i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, (count_down_at > i)? Wheel(100) : 0);
      }
      //drinkLeds.setPixelColor(0, (count_down_at % 2)? Wheel(100) : 0);
    } else if (state == STATE_DANCING) {
      uint16_t rainbow_cycle = (current >> 3) % 256;
      uint16_t spotlight_cycle = (current >> 2) % 256;
      int16_t baseColor = 100; //(current >> 6) % 256;
      int16_t slowSine = 10*sin((float)(rainbow_cycle*2*PI)/256.0f);
      for (uint8_t i = 0; i < strip.numPixels(); i++) {
        int16_t spotlight_sin = 100*sin((float)(spotlight_cycle*2*PI)/256.0f + i*6) + 100;
        uint32_t color = dim(Wheel((uint32_t)(baseColor + slowSine) % 255), constrain(spotlight_sin,0,255));
        if ((current - dancing_last_time) < 40 && (dancing_last_foot%6 == i)) color = 0xFFFFFF;
        strip.setPixelColor(i, color);
      }
    } else if (state == STATE_POURING) {
      uint16_t rainbow_cycle = (current >> 3) % 256;
      int16_t slowSine = constrain(255 - (30*sin((float)(rainbow_cycle*2*PI)/256.0f) + 30), 0, 255);
      for (uint8_t i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, (i == chosen_drink)? dim(Wheel(100), slowSine) : 0);
      }
    } else if (state == STATE_DONE) {
      uint16_t rainbow_cycle = (current >> 3) % 256;
      int16_t slowSine = 50*sin((float)(rainbow_cycle*2*PI)/256.0f);
      uint16_t spotlight_cycle = (current >> 2) % 256;
      for (uint8_t i = 0; i < strip.numPixels(); i++) {
        int16_t spotlight_sin = 100*sin((float)(spotlight_cycle*2*PI)/256.0f + i*6) + 100;
        uint32_t color = Wheel((uint32_t)(100 + slowSine + spotlight_sin) % 255);
        strip.setPixelColor(i, dim(color, constrain(spotlight_sin/2+50,0,255)));
      }
    } else {
      for (uint8_t i = 0; i < strip.numPixels(); i++)
        strip.setPixelColor(i, 0);
    }
    strip.show();

    //drink led handling
    if (state == STATE_IDLE || state == STATE_POURING) {
      int16_t slowSine = 10*sin((float)(((current >> 3) % 256)*2*PI)/256.0f);
      drinkLeds.setPixelColor(0, Wheel((125 + slowSine) % 256));
    } else if (state == STATE_DONE) {
      drinkLeds.setPixelColor(0, 0xFFFFFF);
      //drinkLeds.setPixelColor(0, (count_down_at % 2)? Wheel(100) : 0);
    } else {
      drinkLeds.setPixelColor(0, 0);
    }
    drinkLeds.show();
  }
}


enum pour_states {
  POUR_INIT, 
  POUR_ING,
  
  STIR_INIT,
  STIR_DROP,
  STIR_STIRRING,
  STIR_FINISH,
};
#define DELAY_AFTER_POUR 500
#define SERVO_TRAVEL_TIME 800

uint8_t pour_and_stir(const uint8_t drink, const int32_t pour_time) {
  static uint8_t stir_state = POUR_INIT;
  static int16_t max_delay = 0;
  static int16_t amounts[PUMP_PINS_COUNT] = {0};

  if (stir_state == POUR_INIT) {
    stir_state = POUR_ING;
    for (uint8_t pump=0; pump < PUMP_PINS_COUNT; pump++) {
      const float amount = recipes[drink][pump];
      //amounts[pump] = (amount > 0)? round((amount * 4977.7) - 1318.6) : 0;
      amounts[pump] = mapf(amount, 0, 3.4f, 0, 11612);
      max_delay = max(amounts[pump], max_delay);
      if (amounts[pump]) p("pouring %s [pump %d] for %d ms (at %d)\n", ingredients[pump], pump, amounts[pump], pour_time);
    }
  }

  if (stir_state == POUR_ING) {
    for (uint8_t pump=0; pump < PUMP_PINS_COUNT; pump++){
      digitalWrite(pumpPins[pump], pour_time < amounts[pump]);
      if (abs(pour_time - amounts[pump]) < 1) p("done @pump %d (at %li)\n", pump, pour_time);
    }
    if (pour_time > (max_delay + DELAY_AFTER_POUR)) {
      stir_state = STIR_INIT;
    }
  }

  if (stir_state >= STIR_INIT) {
    int32_t stir_time = (pour_time - (max_delay + DELAY_AFTER_POUR));
    if (stir_state == STIR_INIT) {
      stir_state = STIR_DROP;
      servo.attach(SERVO_PIN);
      servo.write(110);
      p("dropping (at %d)\n", pour_time);
    } else if ((stir_state == STIR_DROP) && (stir_time > SERVO_TRAVEL_TIME)) {
      stir_state = STIR_STIRRING;
      servo.detach();
      p("stirring (at %d)\n", pour_time);
    } else if ((stir_state == STIR_STIRRING) && (stir_time > SERVO_TRAVEL_TIME + TIME_TO_STIR)) {
      stir_state = STIR_FINISH;
      servo.attach(SERVO_PIN);
      servo.write(0);
      p("moving stirrer up (at %d)\n", pour_time);
    } else if ((stir_state == STIR_FINISH) && (stir_time > SERVO_TRAVEL_TIME + TIME_TO_STIR + SERVO_TRAVEL_TIME)) {
      stir_state = POUR_INIT;
      servo.detach();
      p("done stiring (at %d)\n", pour_time);
      return 1;
    }
  }
  return 0;
}


// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } 
  else if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } 
  else {
    WheelPos -= 170;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

uint32_t dim(uint32_t color, uint8_t bright_r, uint8_t bright_g, uint8_t bright_b) {
  uint32_t r = map((color >> 16 & 0xFF), 0, 255, 0, bright_r);
  uint16_t g = map((color >> 8  & 0xFF), 0, 255, 0, bright_g);
  uint16_t b = map((color & 0xFF), 0, 255, 0, bright_b);
  return (r << 16) + (g << 8) + b;
}

uint32_t dim(uint32_t color, uint8_t bright) {
  return dim(color, bright, bright, bright);
}

void p(char *fmt, ... ){
        char tmp[128]; // resulting string limited to 128 chars
        va_list args;
        va_start (args, fmt );
        vsnprintf(tmp, 128, fmt, args);
        va_end (args);
        Serial.print(tmp);
}

float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

