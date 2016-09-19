#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define VERSION "0.2(a)"

static byte	bat_icon[8]	= {
	B01110,
	B11111,
	B11111,
	B11111,
	B11111,
	B11111,
	B11111,
	B11111
};

static byte	bat_in_icon[8]	= {
	B01110,
	B11111,
	B10101,
	B10101,
	B10101,
	B10001,
	B10101,
	B11111
};

LiquidCrystal_I2C lcd(0x27,16,2);	// Инициализация дисплея (адрес, количество символов, количество строк)

struct sys_conf {
	byte	password[6],		// Пароль, для отключения сигнала
			red_speed,			// Включать ли красный свет при уменьшении скорости
			time_to_slpeep,		// Время, после которого компьютер уснёт (минуты)
			auto_led,			// Включать подсветку экрана по датчику света или нет
			auto_headlights;	// Включать фару по датчику света или нет

	float	distance,
			max_speed;
};

/*Системные макросы*/
#define BUTTON_PRESS	LOW

/*Стандартные значения*/
#define WHEEL_LENGTH	2.050	// Длина окружности колеса в метрах
#define BIP_DELAY		4		// Сколько итераций пищать (* 500 / 10000)

/*Аналоговые выводы*/
#define ACTION_BUTTON	2
#define BUTTON_OK		256
#define BUTTON_LEFT		680
#define BUTTON_RIGHT	341

/*Цифровые выводы*/
#define BIP				4
#define BUTTON_CANCEL	341
#define BUTTON_SIGNAL	341

bool			alarm_state		= false,	// Для сигнализации
				bat_external	= true;

unsigned long	lastturn;	// Время последнего обращения

float			speed_now		= 0.0,	// Скорость
				distance		= 0.0;	// Расстояние

byte			bat_percent		= 100,
				system_state	= 0;

#define LOAD_STATE		0
#define SPEED_STATE		1
#define MENU_STATE		2
#define SETTINGS_STATE	3

sys_conf		btc_config;	// Конфигурация системы

unsigned int key_pressed() {
	int key;

	while (true) {

		while ((key = analogRead(ACTION_BUTTON)) > 1000);

		#ifdef DEBUG
		Serial.println(key);
		#endif

		if ((key >= BUTTON_OK - 10) && (key <= BUTTON_OK + 10)) {
			key	= BUTTON_OK;
			break;
		} else if ((key >= BUTTON_LEFT - 10) && (key <= BUTTON_LEFT + 10)) {
			key	= BUTTON_LEFT;
			break;
		} else if ((key >= BUTTON_RIGHT - 10) && (key <= BUTTON_RIGHT + 10)) {
			key	= BUTTON_RIGHT;
			break;
		}
	}

	while (analogRead(ACTION_BUTTON) < 1000);

	delay(10);

	return key;
}

void road_mode() {	// Режим спидометра
	float	speed_old	= 0.0;

	int		wait_key	= 0;

	lcd.clear();
	lcd.print("km/h: ");

	lcd.setCursor(0, 1);
	lcd.print("km:");

	lcd.setCursor(15, 1);
	lcd.write(0);

	while (system_state	== SPEED_STATE) {
		if (speed_old > speed_now) {
			lcd.setCursor(5, 0);
			lcd.print("    ");
		}
		lcd.setCursor(5, 0);
		lcd.print(speed_now);
		speed_old	= speed_now;

		lcd.setCursor(11, 0);
		lcd.print("00:00");

		lcd.setCursor(3, 1);
		lcd.print(distance);

		lcd.setCursor(12, 1);
		if (bat_percent == 100) {
			lcd.print("100");
		} else if ((bat_percent > 100) && (bat_percent <= 10)){
			lcd.print(" ");
			lcd.print(bat_percent);
		} else if (bat_percent > 10){
			lcd.print("  ");
			lcd.print(bat_percent);
		}

		for (wait_key	= 0; wait_key < 500; wait_key++, delay(1)) {
			if ((analogRead(ACTION_BUTTON) >= BUTTON_OK - 10) && (analogRead(ACTION_BUTTON) <= BUTTON_OK + 10)) {
				while (analogRead(ACTION_BUTTON) < 1000);
				delay(50);
				lcd.clear();
				system_state	= MENU_STATE;
				break;
			}
		}
	}
}

byte display_list(byte all_element, char settings_array[][15]) {
	lcd.clear();

	byte	selected		= 0,
			on_display		= 0;

	bool	key_pressed_ok	= true;

	while (true) {
		if (key_pressed_ok) {
			if (selected == on_display) {
				lcd.setCursor(0,0);
				lcd.print(">");
				lcd.setCursor(0,1);	
				lcd.print(" ");
			} else {
				lcd.setCursor(0,1);	
				lcd.print(">");
				lcd.setCursor(0,0);
				lcd.print(" ");
			}

			lcd.setCursor(1,0);
			lcd.print(settings_array[on_display]);

			if (on_display + 1 < all_element) {
				lcd.setCursor(1,1);
				lcd.print(settings_array[on_display + 1]);
			}
		}

		key_pressed_ok	= true;

		switch (key_pressed()) {

			case BUTTON_OK:		return selected;
								break;

			case BUTTON_LEFT:	if (selected != 0) {
									if (selected == on_display) {
										lcd.clear();
										on_display	-= 2;
									}

									selected--;
								}
								break;

			case BUTTON_RIGHT:	if (selected < all_element - 1) {
									if (selected == (on_display + 1)) {
										lcd.clear();
										on_display	+= 2;
									}

									selected++;
								}
								break;

			default:			key_pressed_ok	= false;
								break;
		}
	}
}

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

	lcd.createChar(0, bat_icon);

	#ifdef FIRST_ON
	sys_conf load_to_eeprom;

	load_to_eeprom.password[0]		= 1;
	load_to_eeprom.password[1]		= 2;
	load_to_eeprom.password[2]		= 3;
	load_to_eeprom.password[3]		= 4;
	load_to_eeprom.red_speed		= true;
	load_to_eeprom.time_to_slpeep	= 5;
	load_to_eeprom.auto_led			= true;
	load_to_eeprom.auto_headlights	= true;
	load_to_eeprom.distance			= 0.0;
	load_to_eeprom.max_speed		= 0.0;

	WriteSysConfEEPROM(load_to_eeprom, 0);
	lcd.print("All OK!");
	#endif
}

void WriteSysConfEEPROM(sys_conf str, int base) {	// Запись и чтение выполнены великолепным программистом Виктором Охотниковым
	char	*c;
	int		structSize = sizeof(sys_conf);

	c	= (char *) &str;

	for(int i = 0; i < structSize; i++) {
		EEPROM.write(i+base, *c);
		c++;
	}
}

void ReadSysConfEEPROM(sys_conf *str, int base) {
	char	*c;
	int		structSize = sizeof(sys_conf);

	c	= (char *) str;

	for(int i = 0; i < structSize; i++) {
		*c = EEPROM.read(i+base);
		c++;
	}
}

void loop() {
	if (system_state == LOAD_STATE) {
		lcd.clear();
		lcd.print("Loading system..");
		lcd.setCursor(0, 1);
		lcd.print("Version: ");
		lcd.print(VERSION);
		delay(3000);
		system_state	= SPEED_STATE;
	}

	if (system_state == SPEED_STATE) {
		road_mode();
	}

	if (system_state == MENU_STATE) {
		char menu_array[3][15]	= {
			"Speedometr",
			"Statistik",
			"Settings"
		};

		switch (display_list(3, menu_array)) {
			case 0:	system_state	= SPEED_STATE;
					break;

			case 2:	system_state	= SETTINGS_STATE;
					break;
		}
	}

	if (system_state == SETTINGS_STATE) {
		char settings_array[3][15]	= {
			"Password",
			"About system",
			"Back"
		};

		switch (display_list(3, settings_array)) {
			case 0:	input_password();
					break;

			case 2:	system_state	= MENU_STATE;
					break;
		}
	}
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
	lcd.print("New password");

	lcd.setCursor(1, 0);	// Установка указателя в левый верхний угол
	lcd.print("Password: ");

	for (byte	i	= 0; i < 6; i++) {
		btc_config.password[i]	= key_pressed();
		lcd.print("*");
	}

	lcd.clear();
	lcd.print("Complete!");
	delay(2000);
}

bool read_password() {
	bool	wrong	= false;

	lcd.clear();			// Очистка экрана
	lcd.print("Current password");
	
	lcd.setCursor(1, 0);	// Установка указателя в левый верхний угол
	lcd.print("Password: ");

	for (byte	i	= 0; i < 6; i++) {
		if (btc_config.password[i]	!= key_pressed()) {
			wrong	= true;
		}

		lcd.print("*");
	}

	lcd.clear();
	if (wrong) {
		lcd.print("Incorrect!");
	} else {
		lcd.print("Pass!");
	}

	delay(2000);
}