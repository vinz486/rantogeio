// ---- FEATURES -------------------


#define ENABLE_WIFI 1
#define ENABLE_SNTP 1
#define ENABLE_BUTTONS 1
#define ENABLE_OTA 1
#define ENABLE_GPS 0

// ---- PIN MAPPING ----------------

#define PIN_LEFT_BUTTON  GPIO_NUM_26
#define PIN_RIGHT_BUTTON GPIO_NUM_23

#define PIN_HOUR_STEP    GPIO_NUM_21
#define PIN_HOUR_SLEEP   GPIO_NUM_22
#define PIN_MINUTE_STEP  GPIO_NUM_18
#define PIN_MINUTE_SLEEP GPIO_NUM_19

#define PIN_CASIO        GPIO_NUM_16

// ---- OPTIONS --------------------

// Default values for base steps - these will be overridden by stored settings
extern int HOUR_STEPS_BASE;
extern int MINUTE_STEPS_BASE;
#define MICROSTEPPING_MULTIPLIER 16
#define STEP_INTERVAL 54
#define SIMULATE_12_HOUR 0

#define CASIO_FREQUENCY 4000
#define CASIO_DURATION  200
#define CASIO_INTERVAL  50

#define GPS_HARDWARE_SERIAL 2 // TX 17, RX 16
#define GPS_BAUD 9600

#define TEMPLATE_PLACEHOLDER '$'
// ---------------------------------
