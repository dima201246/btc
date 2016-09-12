#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);  // Устанавливаем дисплей


#define BIP 4

bool alarm_state = false;
int alarm_delay = 0;

void setup() {
	#ifdef DEBUG
	Serial.begin(9600);
	#endif

	// lcd.begin();
	// lcd.backlight();

	pinMode(BIP, OUTPUT);
	digitalWrite(BIP, LOW);

	// lcd.print("Interrupt test");

	attachInterrupt(0, alarm, CHANGE);	// Прерывание на 2-ом цифровом канале
}

void loop() {
	if (alarm_state) {
		digitalWrite(BIP, HIGH);
		delay(250);
		digitalWrite(BIP, LOW);
		delay(250);

		if (alarm_delay == 10) {
			alarm_delay	= 0;
			alarm_state	= false;
		} else {
			alarm_delay++;
		}
	}
}

void alarm() {
	if (!alarm_state)
		alarm_state	= true;
}