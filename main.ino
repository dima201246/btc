/*
	BLUE SPARK PROJECT
*/

#include <EEPROM.h>
#include <Time.h>
#include <Wire.h>
#include <DS1307RTC.h>
#include <LiquidCrystal_I2C.h>

// #define FIRST_LOADING
// #define DEBUG
// #define ENGINEERING_MODE
#define TIKTOK 4

#define set_bit(m,b)	((m) |= (b))
#define unset_bit(m,b)	((m) &= ~(b))
#define bit_seted(m,b)	((b) == ((m) & (b)))

#define byte_to_analog(x)	((int)(4.015 * x))
#define analog_to_byte(x)	((byte)(0.248 * x))

LiquidCrystal_I2C lcd(0x27,16,2); // Инициализация дисплея (адрес, количество символов, количество строк)

#define VERSION "0.5(r)"

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

static byte	signal_icon[8] = {
	B00001,
	B00011,
	B11111,
	B11111,
	B11111,
	B11111,
	B00011,
	B00001
};

static byte	lock_icon[8] = {
	B01111,
	B01111,
	B00011,
	B00111,
	B00011,
	B11111,
	B11011,
	B11111
};

/*bits system_byte*/
#define RED_SPEED			(1 << 0)	// Включать ли красный свет при уменьшении скорости
#define AUTO_BACKLIGHT		(1 << 1)	// Включать подсветку экрана по датчику света или нет
#define AUTO_HEADLIGHT		(1 << 2)	// Включать фару по датчику света или нет
#define NEON_ON_DOWNTIME	(1 << 3)	// Плавное свечение нижней подсветки в простое
#define ALARM_STATE			(1 << 4)	// Для того, чтобы даже после сброса сигнализация не выключилась
#define ALARM_WRITED		(1 << 5)	// Было ли записано значение о сигнализации в EEPROM

struct sys_conf {
	byte	password[6],		// Пароль, для отключения сигнала
			button_ok,
			button_up,
			button_down,
			button_signal,
			button_ok_signal,	// Комбинация кнопок для захода в инженерное меню
			time_to_slpeep,		// Время, после которого компьютер уснёт (минуты)
			system_byte,		// Байт взявший в себя функцию нескольких болевкских переменных
			lux_light_on,		// При какой яркости включать фару
			lux_backlight_on,	// При какой яркости включать подсветку экрана
			bright_headlight;		// Яркость фары

	float	distance,
			max_speed,
			wheel_length;		// Длина колеса
};

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
#define SWITCH_NEXT			200		// Врямя в мс до переключения на следующий пункт при зажатой кнопке
#define LONG_PRESS			2000	// Время долгого нажатия кнопки
#define TURN_SIGNAL_DELAY	500		// Время задержки мигания поворотника

/*Системные define*/
#define	BUT_OK				1
#define	BUT_UP				2
#define	BUT_DOWN			3
#define	BUT_SIGNAL			4
#define BATTERY_ICON		0
#define SIGNAL_ICON			1
#define LOCK_ICON			2

/*Для определения заряда батарей*/
#define TYPVBG				1.1
#define R1_P0				469.3
#define R2_P0				2254.0
#define R1_P1				467.4
#define R2_P1				2195.0
#define VCC					5.02

/*bits turn_signals*/
#define TURN_SIGNAL_ON		(1 << 0)
#define TURN_SIGNAL_LEFT	(1 << 1)
#define TURN_SIGNAL_RIGHT	(1 << 2)
#define SIGNAL_LEFT_ON		(1 << 3)
#define SIGNAL_RIGHT_ON		(1 << 4)
#define EMERGENCY_SIGNAL	(1 << 5)
#define SECOND_ITERATION	(1 << 6)
#define BIP_SIGNAL			(1 << 7)

unsigned long	lastturn;	// Время последнего обращения

float		speed_now		= 0.0,	// Скорость
			distance		= 0.0;	// Расстояние

byte		system_mode		= 0,
			deep_sleep		= false;

/*system_mode*/
#define SPEED_MODE			0
#define ALARM_MODE			1
#define MENU_MODE			2
#define SETTING_MODE		3
#define SLEEP_MODE			4

sys_conf		btc_config; // Конфигурация системы

void setup() {
	#ifdef DEBUG
	Serial.begin(9600);
	#endif

	pinMode(EXT_ALARM, OUTPUT);
	digitalWrite(EXT_ALARM, HIGH);

	pinMode(DISPLAY_LIGHT, OUTPUT);		// Подключение подсветки экрана

	lcd.begin();
	lcd.print("Loading system...");
	lcd.setCursor(0, 1);
	lcd.print("Version: ");
	lcd.print(VERSION);

	for (byte	i	= 0; i < 255; i++, delay(8), analogWrite(DISPLAY_LIGHT, i));


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

	setSyncProvider(RTC.get);			// Для времени

	/*Прерывания*/
	attachInterrupt(0, alarm_sense, CHANGE);
	attachInterrupt(1, speed_sense, RISING);

	analogReference(DEFAULT);	// DEFAULT INTERNAL использовать Vcc как AREF

	lcd.createChar(BATTERY_ICON, bat_icon);	// Загрузка иконки батареи
	lcd.createChar(SIGNAL_ICON, signal_icon);	// Загрузка иконки батареи
	lcd.createChar(LOCK_ICON, lock_icon);	// Загрузка иконки батареи

	#ifdef DEBUG
	lcd.clear();
	lcd.print("Wait for serial");
	lcd.setCursor(0, 1);
	lcd.print("Input \"0\"");
	while (Serial.available() != 0);
	#endif

	#ifdef FIRST_LOADING
	btc_config	= {};
	btc_config.time_to_slpeep	= 5;
	btc_config.bright_headlight	= 254;
	calibration();
	input_password();
	WriteSysConfEEPROM(btc_config, 0);
	#else
	ReadSysConfEEPROM(&btc_config, 0);
	#endif

	#ifdef DEBUG
	Serial.println("btc_config:");
	Serial.print("btc_config.password[0]: ");
	Serial.println(btc_config.password[0]);

	Serial.print("btc_config.password[1]: ");
	Serial.println(btc_config.password[1]);

	Serial.print("btc_config.password[2]: ");
	Serial.println(btc_config.password[2]);

	Serial.print("btc_config.password[3]: ");
	Serial.println(btc_config.password[3]);

	Serial.print("btc_config.password[4]: ");
	Serial.println(btc_config.password[4]);

	Serial.print("btc_config.password[5]: ");
	Serial.println(btc_config.password[5]);

	Serial.print("btc_config.button_ok: ");
	Serial.println(btc_config.button_ok);

	Serial.print("btc_config.button_up: ");
	Serial.println(btc_config.button_up);

	Serial.print("btc_config.button_down: ");
	Serial.println(btc_config.button_down);

	Serial.print("btc_config.button_signal: ");
	Serial.println(btc_config.button_signal);

	Serial.print("btc_config.button_ok_signal: ");
	Serial.println(btc_config.button_ok_signal);

	Serial.print("btc_config.time_to_slpeep: ");
	Serial.println(btc_config.time_to_slpeep);

	Serial.print("btc_config.system_byte: ");
	Serial.println(btc_config.system_byte);

	Serial.print("btc_config.lux_light_on: ");
	Serial.println(btc_config.lux_light_on);

	Serial.print("btc_config.lux_backlight_on: ");
	Serial.println(btc_config.lux_backlight_on);

	Serial.print("btc_config.lux_headlight: ");
	Serial.println(btc_config.bright_headlight);

	Serial.print("btc_config.distance: ");
	Serial.println(btc_config.distance);

	Serial.print("btc_config.max_speed: ");
	Serial.println(btc_config.max_speed);

	Serial.print("btc_config.wheel_length: ");
	Serial.println(btc_config.wheel_length);
	#endif

	#ifdef ENGINEERING_MODE
	if ((analog_to_byte(analogRead(ACTION_BUTTON)) > (btc_config.button_ok_signal - 5)) && (analog_to_byte(analogRead(ACTION_BUTTON)) > (btc_config.button_ok_signal - 5))) {
		engineering_menu();
	}
	#endif

	registerWrite(0);

	system_mode	= SPEED_MODE;
}

void loop() {
	switch (system_mode) {
		case SPEED_MODE:	road_mode();
							break;

		case ALARM_MODE:	break;

		case MENU_MODE:		menu();
							break;

		case SETTING_MODE:	settings();
							break;

		case SLEEP_MODE:	system_sleep();
							break;
	}
}

void alarm_sense() {
	set_bit(btc_config.system_byte, ALARM_STATE);
}

void speed_sense() {	// Подсчёт скорости
	system_mode	= SPEED_MODE;

	if ((millis() - lastturn) > 80) { // Защита от случайных измерений (основано на том, что велосипед не будет ехать быстрее 120 кмч)
		speed_now	= btc_config.wheel_length / ((float)(millis() - lastturn) / 1000) * 3.6;	// Расчет скорости, км/ч
		lastturn	= millis();	// Запомнить время последнего оборота
		distance	= distance + btc_config.wheel_length / 1000;	// Прибавляем длину колеса к дистанции при каждом обороте
	}
}

bool sys_watch() {
	if ((system_mode != SLEEP_MODE) && (bit_seted(btc_config.system_byte, AUTO_HEADLIGHT)) && (analog_to_byte(analogRead(LIGHT_SENSOR)) > btc_config.lux_light_on)) {	// Следилка за фарой
		digitalWrite(HEADLIGHT, LOW);
	} else if (system_mode != SLEEP_MODE) {
		analogWrite(HEADLIGHT, btc_config.bright_headlight);
	}

	if ((system_mode != SLEEP_MODE) && (bit_seted(btc_config.system_byte, AUTO_BACKLIGHT)) && (analog_to_byte(analogRead(LIGHT_SENSOR)) > btc_config.lux_backlight_on)) {	// Следилка за подсветкой экрана
		digitalWrite(DISPLAY_LIGHT, LOW);
	} else if (system_mode != SLEEP_MODE) {
		digitalWrite(DISPLAY_LIGHT, HIGH);
	}

	if ((system_mode != SLEEP_MODE) && (system_mode != SLEEP_MODE) && (bit_seted(btc_config.system_byte, AUTO_BACKLIGHT) == false) && (digitalRead(DISPLAY_LIGHT) == LOW)) {	// Чтобы при выключенной автоподсветке включилась посветка экрана
		digitalWrite(DISPLAY_LIGHT, HIGH);
	}

	if ((deep_sleep) && (system_mode == SPEED_MODE)) {
		return false;
	}

	if ((deep_sleep) && (analogRead(ACTION_BUTTON) > 5)) {
		return false;
	}

	return true;
}

byte key_pressed(bool wait_keys /*Надо ли ожидать кнопку бесконечно*/) {	// Определение нажатой кнопки
	int		key_now;
	byte	delay_time	= 0;	// Время зажатия кнопки

	while (true) {

		if (!sys_watch()) return 0;

		key_now	= analogRead(ACTION_BUTTON);
		
		#ifdef DEBUG
		Serial.println("****");
		Serial.print("Key pressed: ");
		Serial.println(key_now);
		#endif

		if (key_now < 5) {
			if (!wait_keys) {
				break;
			} else {
				continue;
			}
		}

		key_now	= analog_to_byte(key_now);

		#ifdef DEBUG
		Serial.println(key_now);
		#endif

		if ((key_now >= btc_config.button_ok - 5) && (key_now <= btc_config.button_ok + 5)) {
			key_now = BUT_OK;
			#ifdef DEBUG
			Serial.println("Pressed key ok");
			#endif
			break;
		} else if ((key_now >= btc_config.button_up - 5) && (key_now <= btc_config.button_up + 5)) {
			#ifdef DEBUG
			Serial.println("Pressed key up");
			#endif
			key_now = BUT_UP;
			break;
		} else if ((key_now >= btc_config.button_down - 5) && (key_now <= btc_config.button_down + 5)) {
			#ifdef DEBUG
			Serial.println("Pressed key down");
			#endif
			key_now = BUT_DOWN;
			break;
		} else if ((key_now >= btc_config.button_signal - 5) && (key_now <= btc_config.button_signal + 5)) {
			#ifdef DEBUG
			Serial.println("Pressed key signal");
			#endif
			key_now = BUT_SIGNAL;
			break;
		}

		if (!wait_keys)
			break;
	}

	while ((wait_keys) && (analogRead(ACTION_BUTTON) > 5) && (delay_time < SWITCH_NEXT)) {	// Ожидание зажатия
		delay(1);
		delay_time++;
	}

	#ifdef DEBUG
	Serial.println("****");
	#endif

	return key_now;
}

void about() {
	lcd.clear();
	lcd.print("BlueSparkProject");
	lcd.setCursor(0, 1);
	lcd.print("Version: ");
	lcd.print(VERSION);
	key_pressed(true);

	lcd.clear();
	lcd.print("Build time:");
	lcd.print(__TIME__);
	lcd.setCursor(0, 1);
	lcd.print("Date:");
	lcd.print(__DATE__);
	key_pressed(true);

	lcd.clear();
	lcd.print(":DV company 2016");
	lcd.setCursor(0, 1);
	lcd.print("dima201246");
	key_pressed(true);
}

void menu() {
	char menu_array[][15]	= {
		"Sleep",
		"Speedometr",
		"Stopwatch",
		"System state",
		"Settings",
		"Back"
	};

	while ((system_mode == MENU_MODE) && (sys_watch())) {
		switch(display_list(menu_array, 6)) {
			case 0:	system_mode	= SLEEP_MODE;
					break;

			case 1:	system_mode	= SPEED_MODE;
					break;

			case 2:	break;

			case 3:	lcd.clear();
					lcd.print("Internal: ");
					lcd.print(bat_state(0));
					lcd.print("%");
					lcd.setCursor(0, 1);
					lcd.print("External: ");
					lcd.print(bat_state(1));
					lcd.print("%");
					key_pressed(true);
					break;

			case 4:	system_mode	= SETTING_MODE;
					break;

			default:	system_mode	= SPEED_MODE;
						break;
		}
	}
}

void settings() {
	bool	changes	= false;	// Для отслеживания изменений и последующей записи в EEPROM
	byte	count = 0;

	char setting_array[][15]	= {
		"Blue light",
		"Password",
		"Light settings",
		"Time setting",
		"Wheel length",
		"About system",
		"Back"
	};

	while ((system_mode	== SETTING_MODE) && (sys_watch())) {
		switch (display_list(setting_array, 7)) {
			case 0: if (digitalRead(LOWER_LIGHT) == HIGH) {
						digitalWrite(LOWER_LIGHT, LOW);
					} else {
						digitalWrite(LOWER_LIGHT, HIGH);
					}
					lcd.clear();
					lcd.print("       OK");
					delay_w(1000);
					break;

			case 1:	while ((read_password() == false) && (count < 3)) {	// Счётчик количества неправильных вводов
						count++;
					}

					if (count >= 3) {
						set_bit(btc_config.system_byte, ALARM_STATE);
						system_mode	= ALARM_MODE;
					} else {
						input_password();
						changes	= true;
					}
					break;

			case 2: if (!changes) changes	= light_settings();
					else light_settings();
					break;

			case 3:	if (!changes) changes	= time_settings();
					else time_settings();
					break;

			case 4:	btc_config.wheel_length	= input_float_number("Wheel length (m)", 0.0, 10.0, btc_config.wheel_length);
					changes	= true;
					break;

			case 5: about();
					break;

			default:	system_mode	= MENU_MODE;
						break;
		}
	}

	if (changes) {
		WriteSysConfEEPROM(btc_config, 0);
	}
}

byte display_list(char list_array[][15], byte all_element) {
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
			lcd.print(list_array[on_display]);

			if (on_display + 1 < all_element) {
				lcd.setCursor(1,1);
				lcd.print(list_array[on_display + 1]);
			}
		}

		key_pressed_ok	= true;

		switch (key_pressed(true)) {
			case BUT_SIGNAL:	return selected;
								break;

			case BUT_OK:	return selected;
							break;

			case BUT_UP:	if (selected != 0) {
								if (selected == on_display) {
									lcd.clear();
									on_display	-= 2;
								}

								selected--;
							}
							break;

			case BUT_DOWN:	if (selected < all_element - 1) {
								if (selected == (on_display + 1)) {
									lcd.clear();
									on_display	+= 2;
								}

								selected++;
							}
							break;

			default:		key_pressed_ok	= false;
		}
	}
}

byte input_int_number(const char *text, byte min_num, byte max_num, byte same_num) {	// Ввод целочисленного числа
	lcd.clear();
	lcd.print(text);

	while (true) {
		lcd.setCursor(0, 1);
		lcd.print("   ");

		lcd.setCursor(0, 1);
		lcd.print(same_num);

		switch (key_pressed(true)) {
			case BUT_UP:	if (same_num == max_num) {
								same_num	= min_num;
							} else {
								same_num++;
							}
							break;

			case BUT_DOWN:	if (same_num == min_num) {
								same_num	= max_num;
							} else {
								same_num--;
							}
							break;

			case BUT_OK:	return same_num;
							break;

			case BUT_SIGNAL:	return same_num;
								break;
		}
	}
}

float input_float_number(const char *text, float min_num, float max_num, float same_num) {	// Ввод числа с точкой
	lcd.clear();
	lcd.print(text);

	while (true) {
		lcd.setCursor(0, 1);
		lcd.print("                ");
		lcd.setCursor(0, 1);
		lcd.print(same_num);

		switch (key_pressed(true)) {
			case BUT_UP:	if ((same_num + 0.1) > max_num) {
									same_num	= min_num;
							} else {
								same_num	+= 0.1;
							}
							break;

			case BUT_DOWN:	if ((same_num - 0.1) < min_num) {
								same_num	= max_num;
							} else {
								same_num	-= 0.1;
							}
							break;

			case BUT_OK:	return same_num;
							break;

			case BUT_SIGNAL:	return same_num;
								break;
		}
	}
}

bool input_int_number(byte min_num, byte max_num, byte &same_num) {	// Ввод целочисленного числа
	lcd.setCursor(0, 1);
	lcd.print("   ");

	lcd.setCursor(0, 1);
	lcd.print(same_num);

	switch (key_pressed(true)) {
		case BUT_UP:	if (same_num == max_num) {
							same_num	= min_num;
						} else {
							same_num++;
						}
						return true;
						break;

		case BUT_DOWN:	if (same_num == min_num) {
							same_num	= max_num;
						} else {
							same_num--;
						}
						return true;
						break;

		case BUT_OK:	return false;
						break;

		case BUT_SIGNAL:	return false;
						break;
	}

	return true;
}

void registerWrite(int whichPin, int whichState) {
	byte bitsToSend = 0;	// инициализируем и обнуляем байт

	digitalWrite(8, LOW);	//Отключаем вывод на регистре
	bitWrite(bitsToSend, whichPin, whichState);	// устанавливаем HIGH в соответствующем бите 
	shiftOut(11, 12, MSBFIRST, bitsToSend);	// проталкиваем байт в регистр
	digitalWrite(8, HIGH);	// "защелкиваем" регистр, чтобы байт появился на его выходах
}

void registerWrite(byte bitsToSend) {
	digitalWrite(8, LOW);	//Отключаем вывод на регистре
	shiftOut(11, 12, MSBFIRST, bitsToSend);	// проталкиваем байт в регистр
	digitalWrite(8, HIGH);	// "защелкиваем" регистр, чтобы байт появился на его выходах
}

void input_password() {
	lcd.clear();			// Очистка экрана
	lcd.print("New password");

	lcd.setCursor(0, 1);	// Установка указателя в левый верхний угол
	lcd.print("Password: ");

	for (byte i = 0; i < 6; i++) {
		btc_config.password[i]	= key_pressed(true);
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
		if (btc_config.password[i]	!= key_pressed(true)) {
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

void road_mode() {	// Режим спидометра
	float	speed_old		= 0.1;

	byte	turn_signals	= 0,
			baterry_percent	= 0;

	int		road_delay;

	lcd.clear();
	lcd.print("km/h: ");

	lcd.setCursor(0, 1);
	lcd.print("km:");

	lcd.setCursor(15, 1);
	lcd.write(BATTERY_ICON);

	#ifdef DEBUG
	int	key_now;
	#endif

	while (system_mode == SPEED_MODE) {
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
		baterry_percent	= bat_state(0); 

		if ((100 > baterry_percent) && (baterry_percent >= 10))
			lcd.print(" ");

		if (baterry_percent < 10)
			lcd.print("  ");

		lcd.print(baterry_percent);


		if (bit_seted(turn_signals, TURN_SIGNAL_ON)) {
			if (bit_seted(turn_signals, TURN_SIGNAL_LEFT)) {
				if ((bit_seted(turn_signals, SIGNAL_LEFT_ON)) && (bit_seted(turn_signals, SECOND_ITERATION))) {
					registerWrite(LEFT_TURN_LIGHT, 0);
					lcd.setCursor(8, 1);
					lcd.print(" ");
				} else if ((bit_seted(turn_signals, SIGNAL_LEFT_ON)) && !(bit_seted(turn_signals, SECOND_ITERATION))) {
					registerWrite(LEFT_TURN_LIGHT, 1);
					lcd.setCursor(8, 1);
					lcd.print("<");
				}
			}

			if (bit_seted(turn_signals, TURN_SIGNAL_RIGHT)) {
				if ((bit_seted(turn_signals, SIGNAL_RIGHT_ON)) && (bit_seted(turn_signals, SECOND_ITERATION))) {
					registerWrite(RIGHT_TURN_LIGHT, 0);
					lcd.setCursor(10, 1);
					lcd.print(" ");
				} else if ((bit_seted(turn_signals, SIGNAL_RIGHT_ON)) && !(bit_seted(turn_signals, SECOND_ITERATION))) {
					registerWrite(RIGHT_TURN_LIGHT, 1);
					lcd.setCursor(10, 1);
					lcd.print(">");
				}
			}

			#ifdef TIKTOK
			if (bit_seted(turn_signals, SECOND_ITERATION)) {
				registerWrite(TIKTOK, 1);
			}
			#endif

		}

		for (road_delay	= 0; road_delay < 500; road_delay++) { 

			switch(key_pressed(false)) {
				case BUT_OK:		system_mode	= MENU_MODE;
									break;

				case BUT_UP:		if (bit_seted(turn_signals, TURN_SIGNAL_RIGHT)) {
										turn_signals	= 0;
										registerWrite(0);
										lcd.setCursor(10, 1);
										lcd.print("  ");
									}
									if (!bit_seted(turn_signals, TURN_SIGNAL_ON)) {
										set_bit(turn_signals, TURN_SIGNAL_ON);
										set_bit(turn_signals, TURN_SIGNAL_LEFT);
										set_bit(turn_signals, SIGNAL_LEFT_ON);
										registerWrite(LEFT_TURN_LIGHT, 1);
										lcd.setCursor(8, 1);
										lcd.print("<");
									}
									break;

				case BUT_DOWN:		if (bit_seted(turn_signals, TURN_SIGNAL_LEFT)) {
										turn_signals	= 0;
										registerWrite(0);
										lcd.setCursor(8, 1);
										lcd.print("   ");
									}
									if (!bit_seted(turn_signals, TURN_SIGNAL_ON)) {
										set_bit(turn_signals, TURN_SIGNAL_ON);
										set_bit(turn_signals, TURN_SIGNAL_RIGHT);
										set_bit(turn_signals, SIGNAL_RIGHT_ON);
										registerWrite(RIGHT_TURN_LIGHT, 1);
										lcd.setCursor(10, 1);
										lcd.print(">");
									}
									break;

				case BUT_SIGNAL:	tone(BIP, 2000, 250);
									set_bit(turn_signals, BIP_SIGNAL);
									lcd.setCursor(9, 1);
									lcd.write(SIGNAL_ICON);
									break;

				default:		if (bit_seted(turn_signals, TURN_SIGNAL_ON)) {
									turn_signals	= 0;
									registerWrite(0);
									lcd.setCursor(8, 1);
									lcd.print("   ");
								}
								break;
			}
			delay(1);
		}

		if (bit_seted(turn_signals, BIP_SIGNAL)) {
			lcd.setCursor(9, 1);
			lcd.print(" ");
		}

		if (bit_seted(turn_signals, TURN_SIGNAL_ON)) {
			if (bit_seted(turn_signals, SECOND_ITERATION)) {
				unset_bit(turn_signals, SECOND_ITERATION);
			} else {
				set_bit(turn_signals, SECOND_ITERATION);
			}
		}
	}

	registerWrite(0);
	while ((analogRead(ACTION_BUTTON) > 5) && (sys_watch()));	// Ожидание отжатия кнопки
}

#if defined(ENGINEERING_MODE) || defined(FIRST_LOADING)
void calibration() {
	byte	key_num = 0;

	while (key_num != 5) {
		while (analogRead(ACTION_BUTTON) > 5);
		lcd.clear();

		if (key_num == 0) {
			lcd.print("Press OK key");
			lcd.setCursor(0, 1);

			while ((btc_config.button_ok = analog_to_byte(analogRead(ACTION_BUTTON))) < 2);

			lcd.print(btc_config.button_ok);
			key_num++;
			continue;
		}

		if (key_num == 1) {
			lcd.print("Press UP key");
			lcd.setCursor(0, 1);

			while ((btc_config.button_up = analog_to_byte(analogRead(ACTION_BUTTON))) < 2);

			lcd.print(btc_config.button_up);
			key_num++;
			continue;
		}

		if (key_num == 2) {
			lcd.print("Press DOWN key");
			lcd.setCursor(0, 1);

			while ((btc_config.button_down = analog_to_byte(analogRead(ACTION_BUTTON))) < 2);

			lcd.print(btc_config.button_down);
			key_num++;
			continue;
		}

		if (key_num == 3) {
			lcd.print("Press SIGNAL key");
			lcd.setCursor(0, 1);

			while ((btc_config.button_signal = analog_to_byte(analogRead(ACTION_BUTTON))) < 2);

			lcd.print(btc_config.button_signal);
			key_num++;
			continue;
		}

		if (key_num == 4) {
			lcd.print("Press SIGNAL+OK");
			lcd.setCursor(0, 1);

			btc_config.button_ok_signal	= 0;
		
			while ((analog_to_byte(analogRead(ACTION_BUTTON)) > 2) || (btc_config.button_ok_signal == 0)) {
				btc_config.button_ok_signal	= analog_to_byte(analogRead(ACTION_BUTTON));
			}

			lcd.print(btc_config.button_ok_signal);
			key_num++;
		}
	}

	while (analogRead(ACTION_BUTTON) > 5);

	lcd.clear();
	btc_config.wheel_length	= input_float_number("Wheel length (m)", 0.0, 10.0, btc_config.wheel_length);

	lcd.clear();
	lcd.print("Calibration end!");
	delay(2000);
}
#endif

#ifdef ENGINEERING_MODE
void self_test() {
	byte	i;

	lcd.clear();
	lcd.print("Light sensor");

	digitalWrite(HEADLIGHT, LOW);
	digitalWrite(LOWER_LIGHT, LOW);
	digitalWrite(RED_BACK_LIGHT, LOW);
	registerWrite(0);

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

	while (true) {
		lcd.setCursor(0, 1);
		lcd.print("    ");
		lcd.setCursor(0, 1);
		lcd.print(analogRead(ACTION_BUTTON));
		delay(500);
		if ((analog_to_byte(analogRead(ACTION_BUTTON)) > (btc_config.button_ok - 2)) && (analog_to_byte(analogRead(ACTION_BUTTON)) < (btc_config.button_ok + 2))) break;
	}

	lcd.clear();
	lcd.print("Back light test");

	while (analogRead(ACTION_BUTTON) > 5);

	i	= 0;

	while (true) {
		lcd.setCursor(0, 1); lcd.print("   "); lcd.setCursor(0, 1); lcd.print(i);
		registerWrite(i, HIGH);
		i++;

		if (i == 3) i	= 0;
		delay(500);
		if ((analogRead(ACTION_BUTTON) > (btc_config.button_ok - 5)) && (analogRead(ACTION_BUTTON) < (btc_config.button_ok + 5))) break;
	}

	registerWrite(0, LOW);

	while (analogRead(ACTION_BUTTON) > 5);

	lcd.clear();
	lcd.print("PWM test");

	i	= 0;

	while (true) {
		analogWrite(HEADLIGHT, i);
		analogWrite(LOWER_LIGHT, i);
		analogWrite(RED_BACK_LIGHT, i);
		lcd.setCursor(0, 1); lcd.print("   "); lcd.setCursor(0, 1); lcd.print(i);
		i++;

		if (i == 255) i	= 0;
		delay(5);
		if ((analogRead(ACTION_BUTTON) > (btc_config.button_ok - 5)) && (analogRead(ACTION_BUTTON) < (btc_config.button_ok + 5))) break;
	}

	registerWrite(254);

	lcd.clear();
	lcd.print("Test end.");

	while (analogRead(ACTION_BUTTON) > 5);
}

void engineering_menu() {
	char en_menu[][15] = {
		"Calibration",
		"Self test",
		"Continue"
	};

	bool	cycle	= true;

	lcd.clear();
	lcd.print("ENGINEERING MODE");

	while (analogRead(ACTION_BUTTON) > 5);

	while (cycle) {
		switch (display_list(en_menu, 3)) {
			case 0:	calibration();
					WriteSysConfEEPROM(btc_config, 0);
					break;

			case 1:	self_test();
					break;

			default:	cycle	= false;
						break;
		}
	}
}
#endif

byte bat_state(byte pin) {
	if (pin == 0) {
		if ((((analogRead(0) * VCC) / 1024.0) / (R2_P0 / (R1_P0 + R2_P0))) < 1.0) {
			return 0;
		} else {
			return (byte)(((((analogRead(0) * VCC) / 1024.0) / (R2_P0 / (R1_P0 + R2_P0))) - 2.4) / 1.8 * 100);
		}
	} else {
		if ((((analogRead(1) * VCC) / 1024.0) / (R2_P1 / (R1_P1 + R2_P1))) < 1.0) {
			return 0;
		} else {
			return (byte)(((((analogRead(1) * VCC) / 1024.0) / (R2_P1 / (R1_P1 + R2_P1))) - 2.4) / 1.8 * 100);
		}
	}

	return 0;
}

bool time_settings() {
	char time_set_array[][15] = {
		"Clock setting",
		"Sleep time",
		"Back"
	};

	bool	changes	= false;

	while (sys_watch()) {
		switch (display_list(time_set_array, 3)) {
			case 0:	TimeElements	t_n;
					time_t			timeVal;
					t_n.Second	= 0;
					t_n.Hour	= input_int_number("Input hour", 0, 23, hour());
					t_n.Minute	= input_int_number("Input minutes", 0, 59, minute());
					t_n.Day		= input_int_number("Input day", 1, 31, day());
					t_n.Month	= input_int_number("Input month", 1, 12, month());
					t_n.Year	= input_int_number("Input year", 16, 20, year()) + 30;
					timeVal		= makeTime(t_n);
					RTC.set(timeVal);
					setTime(timeVal);
					break;

			case 1:	input_int_number("Input sleep time", 1, 254, btc_config.time_to_slpeep);
					changes	= true;
					break;

			default:	if (changes) return true;
						else return false;
						break;
		}
	}

	if (changes)
		return true;

	return false;
}

bool light_settings() {
	char light_set_array[][15] {
		"Red signal",
		"Auto backlight",
		"Auto headlight",
		"Blue on sleep",
		"Bright hdlight",
		"Lux for screen",
		"Lux for light",
		"Back"
	};

	bool changes	= false;

	byte value		= 0;

	while (sys_watch()) {
		switch (display_list(light_set_array, 8)) {
			case 0:	value	= input_int_number("Red on stop", 0, 1, bit_seted(btc_config.system_byte, RED_BACK_LIGHT));
					if (value != bit_seted(btc_config.system_byte, RED_BACK_LIGHT)) {
						if (value) {
							set_bit(btc_config.system_byte, RED_BACK_LIGHT);
						} else {
							unset_bit(btc_config.system_byte, RED_BACK_LIGHT);
						}

						changes	= true;
					}
					break;

			case 1:	value	= input_int_number("Auto backlight", 0, 1, bit_seted(btc_config.system_byte, AUTO_BACKLIGHT));
					if (value != bit_seted(btc_config.system_byte, AUTO_BACKLIGHT)) {
						if (value) {
							set_bit(btc_config.system_byte, AUTO_BACKLIGHT);
						} else {
							unset_bit(btc_config.system_byte, AUTO_BACKLIGHT);
						}

						changes	= true;
					}
					break;

			case 2:	value	= input_int_number("Auto headlight", 0, 1, bit_seted(btc_config.system_byte, AUTO_HEADLIGHT));
					if (value != bit_seted(btc_config.system_byte, AUTO_HEADLIGHT)) {
						if (value) {
							set_bit(btc_config.system_byte, AUTO_HEADLIGHT);
						} else {
							unset_bit(btc_config.system_byte, AUTO_HEADLIGHT);
						}

						changes	= true;
					}
					break;

			case 3:	value	= input_int_number("Blue on downtime", 0, 1, bit_seted(btc_config.system_byte, NEON_ON_DOWNTIME));
					if (value != bit_seted(btc_config.system_byte, NEON_ON_DOWNTIME)) {
						if (value) {
							set_bit(btc_config.system_byte, NEON_ON_DOWNTIME);
						} else {
							unset_bit(btc_config.system_byte, NEON_ON_DOWNTIME);
						}

						changes	= true;
					}
					break;

			case 4:	value	= btc_config.bright_headlight;
					lcd.clear();
					lcd.print("Bright headlight");
					lcd.setCursor(9, 1);
					lcd.print("Now:");

					while (sys_watch()) {
						lcd.setCursor(13, 1);
						lcd.print("   ");
						lcd.setCursor(13, 1);
						lcd.print(btc_config.bright_headlight);
						analogWrite(HEADLIGHT, btc_config.bright_headlight);
						if (!input_int_number(1, 254, btc_config.bright_headlight))
							break;
					}

					digitalWrite(HEADLIGHT, LOW);

					if (value != btc_config.bright_headlight) {
						changes	= true;
					}

					break;

			case 5:	value	= btc_config.lux_backlight_on;
					lcd.clear();
					lcd.print("Lux for screen");
					lcd.setCursor(9, 1);
					lcd.print("Now:");

					while (sys_watch()) {
						lcd.setCursor(13, 1);
						lcd.print("   ");
						lcd.setCursor(13, 1);
						lcd.print(analog_to_byte(analogRead(LIGHT_SENSOR)));
						if (!input_int_number(0, 254, btc_config.lux_backlight_on))
							break;
					}

					if (value != btc_config.lux_backlight_on) {
						changes	= true;
					}

					break;

			case 6:	value	= btc_config.lux_light_on;
					lcd.clear();
					lcd.print("Lux for hdlight");
					lcd.setCursor(9, 1);
					lcd.print("Now:");

					while (sys_watch()) {
						lcd.setCursor(13, 1);
						lcd.print("   ");
						lcd.setCursor(13, 1);
						lcd.print(analog_to_byte(analogRead(LIGHT_SENSOR)));
						if (!input_int_number(0, 254, btc_config.lux_light_on))
							break;
					}

					if (value != btc_config.lux_light_on) {
						changes	= true;
					}

					break;

			default:	if (changes) return true;
						else return false;
						break;
		}
	}

	if (changes) return true;

	return false;
}

void delay_w(int delay_num) {
	for (int	delay_count	= 0; ((delay_count < delay_num) && (sys_watch())); delay_count++, delay(1));
}

void system_sleep() {
	byte	count;

	registerWrite(0);
	digitalWrite(HEADLIGHT, LOW);
	digitalWrite(DISPLAY_LIGHT, LOW);
	digitalWrite(LOWER_LIGHT, LOW);
	digitalWrite(RED_BACK_LIGHT, LOW);

	deep_sleep	= true;

	while (true) {
		while ((sys_watch()) && (system_mode == SLEEP_MODE)) {
			lcd.clear();

			if (hour() < 10)
				lcd.print(" ");

			lcd.print(hour());
			lcd.print(":");
			lcd.print(minute());

			lcd.print(" ");
			lcd.print(day());
			lcd.print(".");
			lcd.print(month());
			lcd.print(".");
			lcd.print(year());

			lcd.setCursor(1, 1);
			lcd.print("I:");
			lcd.print(bat_state(0));
			lcd.print("%");
			lcd.setCursor(8, 1);
			lcd.print("E:");
			lcd.print(bat_state(1));
			lcd.print("%");

			lcd.setCursor(15, 1);
			lcd.write(LOCK_ICON);

			delay_w(30000);
		}

		if (system_mode == SLEEP_MODE) {
			deep_sleep	= false; // Выход из глубокого сна, чтобы можно было хоть что-нибудь ввести

			while ((sys_watch()) && (analogRead(ACTION_BUTTON) > 5));

			for (count	= 0; ((count < 3) && (!read_password())); count++);

			if (count >= 3) {
				set_bit(btc_config.system_byte, ALARM_STATE);
				system_mode	= ALARM_MODE;
				break;
			} else {
				break; 
			}
		}
	}

	if (system_mode	!= ALARM_MODE)
		system_mode	= SPEED_MODE;

	deep_sleep	= false;
}