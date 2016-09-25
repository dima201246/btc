#include <EEPROM.h>
#include <Time.h>
#include <Wire.h>
#include <DS1307RTC.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2); // Инициализация дисплея (адрес, количество символов, количество строк)

#define VERSION "0.8(a)"
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

	int		BUTTON_OK,
			BUTTON_UP,
			BUTTON_DOWN,
			BUTTON_SIGNAL;

	byte	red_speed,			// Включать ли красный свет при уменьшении скорости
			time_to_slpeep,		// Время, после которого компьютер уснёт (минуты)
			auto_led,			// Включать подсветку экрана по датчику света или нет
			auto_headlights;	// Включать фару по датчику света или нет

	float	distance,
			max_speed;
};

/*Стандартные значения*/
#define WHEEL_LENGTH	0.193	// Длина окружности колеса в метрах
#define BIP_DELAY		4		// Сколько итераций пищать (* 500 / 10000)

/*Аналоговые выводы*/
#define ACTION_BUTTON	6
#define LIGHT_SENSOR	7

/*Цифровые выводы*/
#define	PWM_S1			5
#define	PWM_S2			6
#define	PWM_S3			9
#define BIP				4
#define DISPLAY_LIGHT	10
#define VIBRO_SENSOR	7

/*Системные макросы*/
#define SWITCH_NEXT		150		// Врямя в мс до переключения на следующий пункт при зажатой кнопке
#define LONG_PRESS		2000	// Время долгого нажатия кнопки

bool			alarm_state		= false;	// Для сигнализации

unsigned long	lastturn;	// Время последнего обращения

float			speed_now		= 0.0,	// Скорость
				distance		= 0.0;	// Расстояние

byte			bat_percent		= 100,
				system_state	= 0;

#define SPEED_STATE		0
#define MENU_STATE		1
#define SETTINGS_STATE	2

sys_conf		btc_config; // Конфигурация системы

void setup() {
	#ifdef DEBUG
	Serial.begin(9600);
	#endif

	/*Инициализация дисплея*/
	lcd.begin();
	pinMode(DISPLAY_LIGHT, OUTPUT);
	pinMode(VIBRO_SENSOR, OUTPUT);
	analogWrite(DISPLAY_LIGHT, 175);	// Включение подсветки

	lcd.print("Loading system..");
	lcd.setCursor(0, 1);
	lcd.print("Version: ");
	lcd.print(VERSION);

	pinMode(PWM_S1, OUTPUT);
	pinMode(PWM_S2, OUTPUT);
	pinMode(PWM_S3, OUTPUT);

	tone(BIP, 100, 250);
	delay(250);
	tone(BIP, 100, 250);
	delay(1000);

	setSyncProvider(RTC.get);			// Для времени

	/*Инициализация выводов для сигнала*/
	pinMode(BIP, OUTPUT);

	/*Прерывание на 2-ом цифровом канале для сигнализации*/
	attachInterrupt(0, alarm, CHANGE);

	/*Прерывание на 3-ем цифровом канале для подсчёта скорости*/
	// attachInterrupt(1, speed, CHANGE);

	lcd.createChar(0, bat_icon);

	#ifdef FIRST_ON
	sys_conf load_to_eeprom;

	load_to_eeprom.password[0]		= 1;
	load_to_eeprom.password[1]		= 2;
	load_to_eeprom.password[2]		= 3;
	load_to_eeprom.password[3]		= 4;
	load_to_eeprom.red_speed		= true;
	load_to_eeprom.time_to_slpeep = 5;
	load_to_eeprom.auto_led		 = true;
	load_to_eeprom.auto_headlights	= true;
	load_to_eeprom.distance		 = 0.0;
	load_to_eeprom.max_speed		= 0.0;

	WriteSysConfEEPROM(load_to_eeprom, 0);
	lcd.print("All OK!");
	#endif
	colibration();
}

/*Interrupt zone start*/
void alarm() {	// Сигнализация
	if ((!alarm_state) && (system_state == 1)) {
		alarm_state = true;

		for (int	i = 0; i < BIP_DELAY; i++) {
			tone(BIP, 750);
			delay(500);
			tone(BIP, 300);
			delay(500);
		}
		noTone(BIP);
		alarm_state = false;
	}
}

void speed() {	// Подсчёт скорости
	if ((millis() - lastturn) > 80) { // Защита от случайных измерений (основано на том, что велосипед не будет ехать быстрее 120 кмч)
		speed_now = WHEEL_LENGTH / ((float)(millis() - lastturn) / 1000) * 3.6; // Расчет скорости, км/ч
		lastturn	= millis();													 // Запомнить время последнего оборота
		distance	= distance + WHEEL_LENGTH / 1000;							 // Прибавляем длину колеса к дистанции при каждом обороте
	}
}
/*Interrupt zone end*/

void loop() {
	first_on();
	if (system_state == SPEED_STATE) {
		road_mode();
	}

	if (system_state == MENU_STATE) {
		#ifdef DEBUG
		char menu_array[6][15]	= {
		#else
		char menu_array[5][15]	= {
		#endif
			"Blue spark",
			"COOL MODE",
			"Speedometr",
			"Statistik",
		#ifdef DEBUG
			"Settings",
			"Self test"
		#else
			"Settings"
		#endif
		};

		#ifdef DEBUG
		switch (display_list(6, menu_array)) {
		#else
		switch (display_list(5, menu_array)) {
		#endif
			case 0:	if (digitalRead(PWM_S2) == LOW) {
						digitalWrite(PWM_S2, HIGH);
					} else {
						digitalWrite(PWM_S2, LOW);
					}
					break;

			case 1: cool_mode();
					break;

			case 2: system_state	= SPEED_STATE;
					break;

			case 4: system_state	= SETTINGS_STATE;
					break;

			#ifdef DEBUG
			case 5: self_test();
					break;
			#endif
		}
	}

	if (system_state == SETTINGS_STATE) {
		char settings_array[6][15]	= {
			"Password",
			"Display",
			"Light settings",
			"Time setting",
			"About system",
			"Back"
		};

		switch (display_list(6, settings_array)) {
			case 0: if (read_password())
						input_password();
					break;

			case 3:	TimeElements	t_n;
					time_t			timeVal;
					t_n.Second	= 0;
					t_n.Hour	= input_int_number("Input hour", 0, 23);
					t_n.Minute	= input_int_number("Input minutes", 0, 59);
					t_n.Day		= input_int_number("Input day", 1, 31);
					t_n.Month	= input_int_number("Input month", 1, 12);
					t_n.Year	= input_int_number("Input year", 15, 20) + 30;
					timeVal		= makeTime(t_n);
					RTC.set(timeVal);
					setTime(timeVal);
					break;

			case 5: system_state	= MENU_STATE;
					break;
		}
	}
}

void cool_mode () {
	lcd.clear();
	lcd.print("   COOL MODE");
	byte light_level;

	digitalWrite(PWM_S1, LOW);

	while (analogRead(ACTION_BUTTON) < 5) {
		for (light_level	= 0; light_level < 255; light_level++, delay(10), analogWrite(PWM_S2, light_level));
		for (light_level	= 255; light_level != 0; light_level--, delay(10), analogWrite(PWM_S2, light_level));
	}

	while (analogRead(ACTION_BUTTON) > 5);
}

void alarm_mode() {
	lcd.clear();
	lcd.print(" Stand By Mode");
	digitalWrite(DISPLAY_LIGHT, LOW);
	digitalWrite(VIBRO_SENSOR, HIGH);
	alarm_state		= false;
	system_state	= 1;
	while (true) {
		while (analogRead(ACTION_BUTTON) < 5);
		if (read_password())
			break;
	}
	digitalWrite(VIBRO_SENSOR, LOW);
}

byte input_int_number(const char *text, byte min_num, byte max_num) {	// Ввод целочисленного числа
	lcd.clear();
	lcd.print(text);

	int		key_now;

	byte	same_num = min_num;

	while (true) {
		lcd.setCursor(0, 1);
		lcd.print("   ");

		lcd.setCursor(0, 1);
		lcd.print(same_num);
		key_now	= key_pressed();

		if (key_now == btc_config.BUTTON_UP) {
			if (same_num == max_num) {
				same_num	= min_num;
			} else {
				same_num++;
			}
		} else if (key_now == btc_config.BUTTON_DOWN) {
			if (same_num == min_num) {
				same_num	= max_num;
			} else {
				same_num--;
			}
		} else if (key_now == btc_config.BUTTON_OK) {
			return same_num;
		}
	}
}

float input_float_number(char text[], float min_num, float max_num) {	// Ввод числа с точкой
	lcd.clear();
	lcd.print(text);

	float	same_num = min_num;

	int		key_now;

	while (true) {
		lcd.setCursor(0, 1);
		lcd.print("                ");
		lcd.setCursor(0, 1);
		lcd.print(same_num);

		key_now	= key_pressed();

		if (key_now == btc_config.BUTTON_UP) {
			if ((same_num + 0.1) > max_num) {
				same_num	= min_num;
			} else {
				same_num	+= 0.1;
			}
		} else if (key_now == btc_config.BUTTON_DOWN) {
			if ((same_num - 0.1) < min_num) {
				same_num	= max_num;
			} else {
				same_num	-= 0.1;
			}
		} else if (key_now == btc_config.BUTTON_OK) {
			return same_num;
		}
	}
}

void sys_watch() {
	if (analogRead(LIGHT_SENSOR) > 600) {
		digitalWrite(DISPLAY_LIGHT, LOW);
		digitalWrite(PWM_S1, LOW);
	} else {
		digitalWrite(DISPLAY_LIGHT, HIGH);
		digitalWrite(PWM_S1, HIGH);
	}
}

void stand_by_mode() {

}

unsigned int key_pressed() {	// Определение нажатой кнопки
	int	key,
		delay_time	= 0;

	while (true) {
		sys_watch();

		key	= analogRead(ACTION_BUTTON);

		#ifdef DEBUG
		Serial.println(key);
		#endif

		if ((key >= btc_config.BUTTON_OK - 5) && (key <= btc_config.BUTTON_OK + 5)) {
			key = btc_config.BUTTON_OK;
			break;
		} else if ((key >= btc_config.BUTTON_UP - 5) && (key <= btc_config.BUTTON_UP + 5)) {
			key = btc_config.BUTTON_UP;
			break;
		} else if ((key >= btc_config.BUTTON_DOWN - 5) && (key <= btc_config.BUTTON_DOWN + 5)) {
			key = btc_config.BUTTON_DOWN;
			break;
		}
	}

	while ((analogRead(ACTION_BUTTON) > 5) && (delay_time < SWITCH_NEXT)) {	// Ожидание зажатия
		delay(1);
		delay_time++;
	}

	return key;
}

void road_mode() {	// Режим спидометра
	float	speed_old = 0.1;

	int		key_now;

	lcd.clear();
	lcd.print("km/h: ");

	lcd.setCursor(0, 1);
	lcd.print("km:");

	lcd.setCursor(15, 1);
	lcd.write(0);

	while (system_state == SPEED_STATE) {
		if (speed_old > speed_now) {
			lcd.setCursor(5, 0);
			lcd.print("		");
		}

		if (speed_old != speed_now) {
			lcd.setCursor(5, 0);
			lcd.print(speed_now);
			speed_old	= speed_now;
		}

		/*TIME START*/
		lcd.setCursor(11, 0);
		
		if (hour() < 10) {
			lcd.print("0");
		}

		lcd.print(hour());
		lcd.print(":");

		lcd.setCursor(14, 0);

		if (minute() < 10) {
			lcd.print("0");
		}

		lcd.print(minute());
		/*TIME END*/

		lcd.setCursor(3, 1);
		lcd.print(distance);

		lcd.setCursor(12, 1);
		if (bat_percent == 100) {
			lcd.print("100");
		} else if ((bat_percent > 100) && (bat_percent <= 10)){
			lcd.print(" ");
			lcd.print(bat_percent);
		} else if (bat_percent > 10){
			lcd.print("	");
			lcd.print(bat_percent);
		}

		key_now	= analogRead(ACTION_BUTTON);

		#ifdef DEBUG
		Serial.print("Key in road_mode: ");
		Serial.println(key_now);
		#endif

		if ((key_now >= btc_config.BUTTON_OK - 5) && (key_now <= btc_config.BUTTON_OK + 5)) {
			while (analogRead(ACTION_BUTTON) > 5);
			delay(50);
			lcd.clear();
			system_state	= MENU_STATE;
			break;
		}

		sys_watch();
	}
}

void first_on() {
	lcd.clear();
	lcd.print("Hello!");

	for (byte	i = 0; i < 255; i++, delay(4)) {
		analogWrite(DISPLAY_LIGHT, i);
	}
}

byte display_list(byte all_element, char settings_array[][15]) {
	lcd.clear();

	byte	selected		= 0,
			on_display		= 0;

	int	 key_now;

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

		key_now = key_pressed();
		Serial.print("key_now: ");
		Serial.println(key_now);

		if (key_now == btc_config.BUTTON_OK) {
			return selected;
		} else if (key_now == btc_config.BUTTON_UP) {
			if (selected != 0) {
				if (selected == on_display) {
					lcd.clear();
					on_display	-= 2;
				}

				selected--;
			}
		} else if (key_now == btc_config.BUTTON_DOWN) {
			if (selected < all_element - 1) {
				if (selected == (on_display + 1)) {
					lcd.clear();
					on_display	+= 2;
				}

				selected++;
			}
		} else {
			key_pressed_ok	= false;
		}
	}
}

void WriteSysConfEEPROM(sys_conf str, int base) { // Запись и чтение выполнены великолепным программистом Виктором Охотниковым
	char	*c;
	int	 structSize = sizeof(sys_conf);

	c = (char *) &str;

	for(int i = 0; i < structSize; i++) {
		EEPROM.write(i+base, *c);
		c++;
	}
}

void ReadSysConfEEPROM(sys_conf *str, int base) {
	char	*c;
	int	 structSize = sizeof(sys_conf);

	c = (char *) str;

	for(int i = 0; i < structSize; i++) {
		*c = EEPROM.read(i+base);
		c++;
	}
}

void input_password() {
	lcd.clear();			// Очистка экрана
	lcd.print("New password");

	lcd.setCursor(0, 1);	// Установка указателя в левый верхний угол
	lcd.print("Password: ");

	for (byte i = 0; i < 6; i++) {
		btc_config.password[i]	= key_pressed();
		lcd.print("*");
	}

	lcd.clear();
	lcd.print("Complete!");
	delay(2000);
}

bool read_password() {
	bool	wrong = false;

	lcd.clear();			// Очистка экрана
	lcd.print("Current password");
	
	lcd.setCursor(0, 1);	// Установка указателя в левый верхний угол
	lcd.print("Password: ");

	for (byte i = 0; i < 6; i++) {
		if (btc_config.password[i]	!= key_pressed()) {
			wrong = true;
		}

		lcd.print("*");
	}

	lcd.clear();
	if (wrong) {
		lcd.print("Incorrect!");
		delay(2000);
		return	false;
	} else {
		lcd.print("Pass!");
		delay(2000);
		return	true;
	}

	return false;
}

void colibration() {
	byte	key_num = 0;

	while (key_num != 3) {
		while (analogRead(ACTION_BUTTON) > 5);
		lcd.clear();

		if (key_num == 0) {
			lcd.print("Press OK key");
			lcd.setCursor(0, 1);

			while ((btc_config.BUTTON_OK = analogRead(ACTION_BUTTON)) < 5);

			lcd.print(btc_config.BUTTON_OK);
			key_num++;
			continue;
		}

		if (key_num == 1) {
			lcd.print("Press UP key");
			lcd.setCursor(0, 1);

			while ((btc_config.BUTTON_UP = analogRead(ACTION_BUTTON)) < 5);

			lcd.print(btc_config.BUTTON_UP);
			key_num++;
			continue;
		}

		if (key_num == 2) {
			lcd.print("Press DOWN key");
			lcd.setCursor(0, 1);

			while ((btc_config.BUTTON_DOWN = analogRead(ACTION_BUTTON)) < 5);

			lcd.print(btc_config.BUTTON_DOWN);
			key_num++;
			continue;
		}

		/*if (key_num == 0) {
			lcd.print("Press OK key");
			lcd.setCursor(1, 0);

			while ((btc_config.BUTTON_OK = analogRead(ACTION_BUTTON)) < 5);

			lcd.print(btc_config.BUTTON_OK);
			key_num++;
		}*/

	}

	while (analogRead(ACTION_BUTTON) > 5);
}

#ifdef DEBUG
void self_test() {
	lcd.clear();
	lcd.print("Light sensor");

	while (analogRead(ACTION_BUTTON) < 5) {
		lcd.setCursor(0, 1);
		lcd.print("    ");
		lcd.setCursor(0, 1);
		lcd.print(analogRead(LIGHT_SENSOR));
		delay(500);
	}

	lcd.clear();
	lcd.print("Key test");

	while (analogRead(ACTION_BUTTON) > 5);

	while (analogRead(ACTION_BUTTON) != btc_config.BUTTON_OK) {
		lcd.setCursor(0, 1);
		lcd.print("    ");
		lcd.setCursor(0, 1);
		lcd.print(analogRead(ACTION_BUTTON));
		delay(500);
	}

	while (analogRead(ACTION_BUTTON) > 5);
}
#endif