/*
	BLUE SPARK PROJECT
*/

#include <EEPROM.h>
#include <Time.h>
#include <Wire.h>
#include <DS1307RTC.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2); // Инициализация дисплея (адрес, количество символов, количество строк)

#define VERSION "1.0(b)"
#define DEBUG

static byte	bat_icon[8] = {
	B01110,
	B11111,
	B11111,
	B11111,
	B11111,
	B11111,
	B11111,
	B11111
};

struct sys_conf {
	int		password[6];		// Пароль, для отключения сигнала

	int		button_ok,
			button_up,
			button_down,
			button_signal,
			lux_light_on,		// При какой яркости включать фару
			lux_backlight_on,	// При какой яркости включать подсветку экрана
			lux_headlight;		// Яркость фары

	bool	red_speed,			// Включать ли красный свет при уменьшении скорости
			auto_backlight,		// Включать подсветку экрана по датчику света или нет
			auto_headlights,	// Включать фару по датчику света или нет
			alarm_state;		// Для того, чтобы даже после сброса сигнализация не выключилась

	byte	time_to_slpeep,		// Время, после которого компьютер уснёт (минуты)

	float	distance,
			max_speed,
			wheel_length;		// Длина колеса
};

/*Стандартные значения*/
#define BIP_DELAY			4	// Сколько итераций пищать (* 500 / 10000)

/*Аналоговые выводы*/
#define ACTION_BUTTON		6
#define LIGHT_SENSOR		7

/*Цифровые выводы*/
#define BIP					4
#define HEADLIGHT			5
#define LOWER_LIGHT			6
#define VIBRO_SENSOR		7
#define LATCH_PIN			8
#define RED_BACK_LIGHT		9
#define DISPLAY_LIGHT		10
#define DATA_PIN			11
#define CLOCK_PIN			12
#define EXT_ALARM			13

/*Выводы на сдвиговом регистре*/
#define LEFT_TURN_LIGHT		0
#define RIGHT_TURN_LIGHT	1

/*Системные макросы*/
#define SWITCH_NEXT			150		// Врямя в мс до переключения на следующий пункт при зажатой кнопке
#define LONG_PRESS			2000	// Время долгого нажатия кнопки

unsigned long	lastturn;	// Время последнего обращения

float		speed_now		= 0.0,	// Скорость
			distance		= 0.0;	// Расстояние

byte		bat_percent		= 100,
			system_state	= 0;

#define SPEED_STATE			0
#define AlARM_STATE			1
#define MENU_STATE			2
#define SLEEP_STATE			3

sys_conf		btc_config; // Конфигурация системы

void setup() {
	pinMode(DISPLAY_LIGHT, OUTPUT);		// Подключение подсветки экрана

	for (byte	i	= 0; i < 255; i++, delay(8), analogWrite(DISPLAY_LIGHT, i));

	lcd.begin();
	lcd.print("Loading system...");
	lcd.setCursor(0, 1);
	lcd.print("Version: ");
	lcd.print(VERSION);

	/*Инициализация цифровых выводов*/
	pinMode(BIP, OUTPUT);
	pinMode(HEADLIGHT, OUTPUT);
	pinMode(LOWER_LIGHT, OUTPUT);
	pinMode(VIBRO_SENSOR, OUTPUT);
	pinMode(LATCH_PIN, OUTPUT);
	pinMode(RED_BACK_LIGHT, OUTPUT);
	pinMode(DISPLAY_LIGHT, OUTPUT);
	pinMode(DATA_PIN, OUTPUT);
	pinMode(CLOCK_PIN, OUTPUT);	
	pinMode(EXT_ALARM, OUTPUT);

	setSyncProvider(RTC.get);			// Для времени

	/*Прерывания*/
	attachInterrupt(0, alarm, CHANGE);
	attachInterrupt(1, speed, CHANGE);

	lcd.createChar(0, bat_icon);	// Загрузка иконки батареи
}

void loop() {

}

void alarm() {

}

void speed() {

}

void sys_watch() {
	if ((btc_config.auto_headlights) && (analogRead(LIGHT_SENSOR) > btc_config.lux_light_on)) {	// Следилка за фарой
		digitalWrite(HEADLIGHT, LOW);
	} else {
		digitalWrite(HEADLIGHT, HIGH);
	}

	if ((btc_config.auto_b) && (analogRead(LIGHT_SENSOR) > btc_config.lux_light_on)) {	// Следилка за фарой
		digitalWrite(DISPLAY_LIGHT, LOW);
}

unsigned int key_pressed(bool wait_keys /*Надо ли ожидать кнопку бесконечно*/) {	// Определение нажатой кнопки
	int		key,
			delay_time	= 0;	// Время зажатия кнопки

	while (sys_watch()) {

		key	= analogRead(ACTION_BUTTON);

		if ((key >= btc_config.button_ok - 5) && (key <= btc_config.button_ok + 5)) {
			key = btc_config.button_ok;
			break;
		} else if ((key >= btc_config.button_up - 5) && (key <= btc_config.button_up + 5)) {
			key = btc_config.button_up;
			break;
		} else if ((key >= btc_config.button_down - 5) && (key <= btc_config.button_down + 5)) {
			key = btc_config.button_down;
			break;
		} else if ((key >= btc_config.button_signal - 5) && (key <= btc_config.button_signal + 5)) {
			key = btc_config.button_signal;
			break;
		}

		if (!wait_keys)
			break;
	}

	while ((analogRead(ACTION_BUTTON) > 5) && (delay_time < SWITCH_NEXT)) {	// Ожидание зажатия
		delay(1);
		delay_time++;
	}

	return key;
}