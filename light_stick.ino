/*
  Light Stick (加油棒)
  Board : Arduino Nano
  Parts : RGB LED module (pins V R B G), push button module (pins IN VCC GND)

  Controls
    Short click  -> next color
    Double-click -> brightness 100% -> 60% -> 30% -> back to 100% (works in every mode)
    Long press   -> next mode (Constant -> Blink -> Breathing -> back to Constant)

  Wiring
    RGB V      -> 5V        Button IN  -> D2
    RGB R      -> D9        Button VCC -> 5V
    RGB B      -> D10       Button GND -> GND
    RGB G      -> D11
    (5V is needed twice, so share it through the breadboard + rail)
*/

// Module pin order is V R B G, so B goes to D10 and G to D11 (wires run straight across)
const int LED_PINS[3] = {9, 11, 10};   // red, green, blue -- all PWM pins
const int BUTTON_PIN = 2;

// The module has a "V" pin, so it is common anode (LOW = on).
// If the LED is on when it should be off, or the colors look wrong, change this to false.
const bool COMMON_ANODE = true;

const unsigned long DEBOUNCE_MS = 30;
const unsigned long LONG_PRESS_MS = 600;     // hold at least this long to switch mode
const unsigned long DOUBLE_CLICK_MS = 300;   // max gap between the two clicks of a double-click
const unsigned long BLINK_MS = 400;          // on time = off time
const unsigned long BREATH_MS = 3000;        // one full breath (bright -> dark -> bright)

const byte COLORS[][3] = {
  {255,   0,   0},   // red
  {  0, 255,   0},   // green
  {  0,   0, 255},   // blue
  {255, 150,   0},   // yellow
  {  0, 255, 255},   // cyan
  {255,   0, 255},   // magenta
  {255, 255, 255},   // white
};

// The eye doesn't see LED brightness linearly, so these PWM fractions
// look roughly like 100%, 60% and 30%.
const float BRIGHTNESS_LEVELS[] = {1.0, 0.35, 0.1};
const char* BRIGHTNESS_NAMES[] = {"100%", "60%", "30%"};

enum Mode { CONSTANT, BLINK, BREATHING, MODE_COUNT };
const char* MODE_NAMES[] = {"Constant", "Blink", "Breathing"};

int mode = CONSTANT;
int colorIndex = 0;
int brightnessIndex = 0;
unsigned long modeStart = 0;

bool idleLevel;              // button reading when NOT pressed, detected at power-on
bool lastReading;
unsigned long lastBounce = 0;
bool isPressed = false;
bool longPressDone = false;
unsigned long pressStart = 0;
int clickCount = 0;          // short clicks waiting to be decided: single or double
unsigned long lastClickTime = 0;

// level: 0.0 = off, 1.0 = full brightness of the current color
void showColor(float level) {
  for (int i = 0; i < 3; i++) {
    int value = COLORS[colorIndex][i] * level * BRIGHTNESS_LEVELS[brightnessIndex];
    analogWrite(LED_PINS[i], COMMON_ANODE ? 255 - value : value);
  }
}

void setup() {
  Serial.begin(9600);
  for (int i = 0; i < 3; i++) pinMode(LED_PINS[i], OUTPUT);
  pinMode(BUTTON_PIN, INPUT);

  // Works whether the button module outputs HIGH or LOW when pressed.
  // Just don't hold the button while plugging in the USB cable.
  idleLevel = digitalRead(BUTTON_PIN);
  lastReading = idleLevel;
  Serial.println("Mode: Constant");
}

void loop() {
  unsigned long now = millis();

  // ---------- button: debounce, then click / double-click / long press ----------
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastReading) {
    lastReading = reading;
    lastBounce = now;
  }

  if (now - lastBounce >= DEBOUNCE_MS) {
    bool down = reading != idleLevel;

    if (down && !isPressed) {                       // just pressed
      isPressed = true;
      longPressDone = false;
      pressStart = now;
    } else if (down && !longPressDone && now - pressStart >= LONG_PRESS_MS) {
      longPressDone = true;                         // long press fires while still holding
      clickCount = 0;                               // and cancels any click still waiting
      mode = (mode + 1) % MODE_COUNT;
      modeStart = now;                              // restart the animation from "on"
      Serial.print("Mode: ");
      Serial.println(MODE_NAMES[mode]);
    } else if (!down && isPressed) {                // just released
      isPressed = false;
      if (!longPressDone) {                         // it was a short click
        clickCount++;
        lastClickTime = now;
        if (clickCount == 2) {                      // double-click: next brightness
          clickCount = 0;
          brightnessIndex = (brightnessIndex + 1) % (sizeof(BRIGHTNESS_LEVELS) / sizeof(BRIGHTNESS_LEVELS[0]));
          Serial.print("Brightness: ");
          Serial.println(BRIGHTNESS_NAMES[brightnessIndex]);
        }
      }
    }
  }

  // A single click only counts once no second click arrived in time
  if (clickCount == 1 && !isPressed && now - lastClickTime > DOUBLE_CLICK_MS) {
    clickCount = 0;
    colorIndex = (colorIndex + 1) % (sizeof(COLORS) / sizeof(COLORS[0]));
  }

  // ---------- light output ----------
  unsigned long t = now - modeStart;

  if (mode == CONSTANT) {
    showColor(1.0);
  } else if (mode == BLINK) {
    showColor((t / BLINK_MS) % 2 == 0 ? 1.0 : 0.0);
  } else {                                          // BREATHING
    float wave = (1 + cos(2 * PI * (t % BREATH_MS) / BREATH_MS)) / 2;   // 1 -> 0 -> 1
    showColor(wave * wave);                         // squared so the fade looks smooth to the eye
  }
}
