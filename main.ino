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

#define set_bit(m,b)	((m) |= (b))
#define unset_bit(m,b)	((m) &= ~(b))
#define bit_seted(m,b)	((b) == ((m) & (b)))

#define byte_to_analog(x)	((int)(4.015 * x))
#define analog_to_byte(x)	((byte)(0.248 * x))

LiquidCrystal_I2C lcd(0x27,16,2); // Инициализация дисплея (адрес, количество символов, количество строк)

#define VERSION "1.0(b)"

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

/*bits system_byte*/
#define RED_SPEED			(1 << 0)	// Включать ли красный свет при уменьшении скорости
#define AUTO_BACKLIGHT		(1 << 1)	// Включать подсветку экрана по датчику света или нет
#define AUTO_HEADLIGHTS		(1 << 2)	// Включать фару по датчику света или нет
#define AlARM_STATE			(1 << 3)	// Для того, чтобы даже после сброса сигнализация не выключилась
#define NENON_ON_DOWNTIME	(1 << 4)	// Плавное свечение нижней подсветки в простое

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
			lux_headlight;		// Яркость фары

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

/*bits turn_signals*/
#define TURN_SIGNAL_ON		(1 << 0)
#define TURN_SIGNAL_LEFT	(1 << 1)
#define TURN_SIGNAL_RIGHT	(1 << 2)
#define SIGNAL_LEFT_ON		(1 << 3)
#define SIGNAL_RIGHT_ON		(1 << 4)
#define EMERGENCY_SIGNAL	(1 << 5)

unsigned long	lastturn;	// Время последнего обращения

float		speed_now		= 0.0,	// Скорость
			distance		= 0.0;	// Расстояние

byte		bat_percent		= 100,
			system_mode		= 0,
			alarm_now		= false,
			turn_signals;

/*system_mode*/
#define SPEED_MODE			0
#define ALARM_MODE			1
#define MENU_MODE			2
#define SLEEP_MODE			3
#define ENGINEERING_MODE	4

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
	attachInterrupt(0, alarm, CHANGE);
	attachInterrupt(1, speed, CHANGE);

	lcd.createChar(0, bat_icon);	// Загрузка иконки батареи

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
	set_bit(btc_config.system_byte, AUTO_BACKLIGHT);
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
	Serial.println(btc_config.lux_headlight);

	Serial.print("btc_config.distance: ");
	Serial.println(btc_config.distance);

	Serial.print("btc_config.max_speed: ");
	Serial.println(btc_config.max_speed);

	Serial.print("btc_config.wheel_length: ");
	Serial.println(btc_config.wheel_length);
	#endif

	if ((analogRead(ACTION_BUTTON) > (btc_config.button_ok_signal - 5)) && (analogRead(ACTION_BUTTON) > (btc_config.button_ok_signal - 5))) {
		engineering_menu();
	}

	system_mode	= SPEED_MODE;
}

void loop() {
	switch (system_mode) {
		case SPEED_MODE:	road_mode();
							break;

		case ALARM_MODE:	break;

		case MENU_MODE:		menu();
							break;

		case SLEEP_MODE:	break;

		case ENGINEERING_MODE:	engineering_menu();
								break;
	}
}

void alarm() {
	alarm_now	= true;
}

void speed() {	// Подсчёт скорости
	if ((millis() - lastturn) > 80) { // Защита от случайных измерений (основано на том, что велосипед не будет ехать быстрее 120 кмч)
		speed_now	= btc_config.wheel_length / ((float)(millis() - lastturn) / 1000) * 3.6;	// Расчет скорости, км/ч
		lastturn	= millis();	// Запомнить время последнего оборота
		distance	= distance + btc_config.wheel_length / 1000;	// Прибавляем длину колеса к дистанции при каждом обороте
	}
}

void(* resetFunc) (void) = 0; // Функция сброса arduino

bool sys_watch() {
	if ((bit_seted(btc_config.system_byte, AUTO_HEADLIGHTS)) && (analogRead(LIGHT_SENSOR) > btc_config.lux_light_on)) {	// Следилка за фарой
		digitalWrite(HEADLIGHT, LOW);
	} else {
		analogWrite(HEADLIGHT, btc_config.lux_headlight);
	}

	if ((bit_seted(btc_config.system_byte, AUTO_BACKLIGHT)) && (analogRead(LIGHT_SENSOR) > btc_config.lux_backlight_on)) {	// Следилка за фарой
		digitalWrite(DISPLAY_LIGHT, LOW);
	} else {
		digitalWrite(DISPLAY_LIGHT, LOW);
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
	lcd.print(__DATE__);
	lcd.print(" ");
	lcd.print(__TIME__);
	key_pressed(true);
}

void menu() {
	char menu_array[][15]	= {
		"Sleep",
		"Speedometr",
		"Stopwatch",
		"System state",
		"Settings"
	};

	while (system_mode == MENU_MODE) {
		switch(display_list(menu_array, 5)) {
			case 0:	break;

			case 1:	system_mode	= SPEED_MODE;
					break;

			case 2:	break;

			case 3:	break;

			case 4:	settings();
					break;
		}
	}
}

void settings() {
	byte changes	= false,	// Для отслеживания изменений и последующей записи в EEPROM
		 count = 0;

	char setting_array[][15]	= {
		"Blue light",
		"Password",
		"Light settings",
		"Time setting",
		"About system",
		"Back"
	};

	while (true) {
		switch (display_list(setting_array, 6)) {
			case 0: if (digitalRead(LOWER_LIGHT) == HIGH) {
						digitalWrite(LOWER_LIGHT, LOW);
					} else {
						digitalWrite(LOWER_LIGHT, HIGH);
					}
					break;

			case 1:	while ((read_password() == false) && (count < 3)) {	// Счётчик количества неправильных вводов
						count++;
					}

					if (count == 3) {
						alarm_now	= true;
					} else {
						input_password();
						changes	= true;
					}
					break;

			case 2: break;

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

			case 4: about();
					break;

			case 5: return;
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

byte input_int_number(const char *text, byte min_num, byte max_num) {	// Ввод целочисленного числа
	lcd.clear();
	lcd.print(text);

	byte	same_num = min_num;

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
		}
	}
}

float input_float_number(char text[], float min_num, float max_num) {	// Ввод числа с точкой
	lcd.clear();
	lcd.print(text);

	float	same_num = min_num;

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
		}
	}
}

byte input_int_number(byte min_num, byte max_num) {	// Ввод целочисленного числа
	byte	same_num = min_num;

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
	}
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
	float	speed_old = 0.1;

	lcd.clear();
	lcd.print("km/h: ");

	lcd.setCursor(0, 1);
	lcd.print("km:");

	lcd.setCursor(15, 1);
	lcd.write(0);

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
		if (bat_percent == 100) {
			lcd.print("100");
		} else if ((bat_percent > 100) && (bat_percent <= 10)){
			lcd.print(" ");
			lcd.print(bat_percent);
		} else if (bat_percent > 10){
			lcd.print("	");
			lcd.print(bat_percent);
		}

		switch(key_pressed(false)) {
			case BUT_OK:		system_mode	= MENU_MODE;
								break;

			case BUT_UP:		break;
			case BUT_DOWN:		break;
			case BUT_SIGNAL:	break;
		}

		#ifdef DEBUG
		delay(500);
		#endif
	}

	while ((analogRead(ACTION_BUTTON) > 5) && (sys_watch()));	// Ожидание отжатия кнопки
}

void calibration() {
	byte	key_num = 0;

	while (key_num != 3) {
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
	lcd.print("Key OK!");
	delay(2000);
}

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
		if ((analogRead(ACTION_BUTTON) > (btc_config.button_ok - 5)) && (analogRead(ACTION_BUTTON) < (btc_config.button_ok + 5))) break;
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
		"Continue work"
	};
}