#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);	// Инициализация дисплея (адрес, количество символов, количество строк)

struct sys_conf {
	byte	password[4],
			param1,
			param2,
			param3;

	float	distance,
			max_speed;
};

/*Системные макросы*/
#define BUTTON_PRESS	LOW

/*Стандартные значения*/
#define WHEEL_LENGTH	2.050	// Длина окружности колеса в метрах
#define BIP_DELAY		4		// Сколько итераций пищать (* 500 / 10000)

/*Цифровые выводы*/
#define BIP				4
#define BUTTON_OK		4
#define BUTTON_LEFT		4
#define BUTTON_RIGHT	4

/*Аналоговые выводы*/


bool			alarm_state = false;	// Для сигнализации

unsigned long	lastturn;	// Время последнего обращения

float			speed_now,	// Скорость
				distance;	// Расстояние

byte			system_state = 0,
				password[4];		// Пароль для отключения сигнализации

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

void WriteSysConfEEPROM(sys_conf str) {
	char	*c;
	int		structSize	= sizeof(sys_conf);

	*c = &str;

	for(int	i	= 0; i < structSize; i++) {
		EEPROM.write(i+base, *c);
		c++;
	}
}

void loop() {
}

void alarm() {	// Сигнализация
	if ((!alarm_state) && (system_state == 1)) {
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

void input_password() {
	lcd.clear();			// Очистка экрана
	lcd.setCursor(0, 0);	// Установка указателя в левый верхний угол
	lcd.print("Password:");
	lcd.setCursor(9, 0);

	for (byte	i	= 0; i < 4; i++) {
		while ((digitalRead(BUTTON_OK) == BUTTON_PRESS) || (digitalRead(BUTTON_LEFT) == BUTTON_PRESS) || digitalRead(BUTTON_RIGHT) == BUTTON_PRESS);	// Ожидание нажатия любой кнопки

		if (digitalRead(BUTTON_OK) == BUTTON_PRESS) {
			password[i]	= BUTTON_OK;
		} else if (digitalRead(BUTTON_LEFT) == BUTTON_PRESS) {
			password[i]	= BUTTON_LEFT;
		} else if (digitalRead(BUTTON_RIGHT) == BUTTON_PRESS) {
			password[i]	= BUTTON_RIGHT;
		}

		lcd.setCursor(9 + i, 0);
		lcd.print("*");
	}

	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("Complete!");
	delay(2000);
}