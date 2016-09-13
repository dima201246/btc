#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);	// Инициализация дисплея (адрес, количество символов, количество строк)

/*Стандартные значения*/
#define WHEEL_LENGTH	2.050	// Длина окружности колеса в метрах
#define BIP_DELAY		4		// Сколько итераций пищать (* 500 / 10000)

/*Цифровые выводы*/
#define BIP 4

bool			alarm_state = false;	// Для сигнализации

unsigned long	lastturn;	// Время последнего обращения

float			speed_now,	// Скорость
				distance;	// Расстояние


void setup() {
	#ifdef DEBUG
	Serial.begin(9600);
	#endif

	/*Инициализация дисплея*/
	lcd.begin();
	// lcd.backlight();

	/*Инициализация выводов для сигнала*/
	pinMode(BIP, OUTPUT);
	digitalWrite(BIP, LOW);

	/*Прерывание на 2-ом цифровом канале для сигнализации*/
	attachInterrupt(0, alarm, CHANGE);

	/*Прерывание на 3-ем цифровом канале для подсчёта скорости*/
	attachInterrupt(1, speed, CHANGE);
}

void loop() {
}

void alarm() {	// Сигнализация
	if (!alarm_state) {
		alarm_state	= true;

		for (int	i = 0; i < BIP_DELAY; i++) {
			digitalWrite(BIP, HIGH);
			delay(250);
			digitalWrite(BIP, LOW);
			delay(250);
		}

		alarm_state	= false;
	}
}

void speed() {	// Подсчёт скорости
	if ((millis() - lastturn) > 80) {	// Защита от случайных измерений (основано на том, что велосипед не будет ехать быстрее 120 кмч)
		speed_now	= WHEEL_LENGTH / ((float)(millis() - lastturn) / 1000) * 3.6;	// Расчет скорости, км/ч
		lastturn	= millis();														// Запомнить время последнего оборота
		distance	= distance + WHEEL_LENGTH / 1000;								// Прибавляем длину колеса к дистанции при каждом обороте
	}
}