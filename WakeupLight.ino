#include <Wire.h>
#include "RTClib.h"
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include "TimerOne.h"
#include <EEPROM.h>
#include "global.h"

#define LEDPIN 6

#define BTNA 2
#define BTNB 3

Adafruit_7segment disp = Adafruit_7segment();


RTC_DS1307 RTC;
DateTime dt_now;
bool btnAState = false;
uint8_t btnADown = 0;
bool btnAHeld = false;
bool btnBState = false;
uint8_t btnBDown = 0;
bool btnBHeld = false;

//Timer1 ticks every 10ms so times below are multiplied by 10ms
#define HOLDTIME 75
#define SETHOLDTIME 15
#define PRESSTIME 10

uint8_t mode = 0;
DateTime setTime;
DateTime alarmTime = DateTime(0, 0, 0, 0, 0, 0);

bool showTime = true;
long timeOn = 0;

bool lightOn = false;
bool dismissed = false;
uint16_t alarmOn = 0;

bool readingMode = false;

#define ALARMSTART 15*60 //starts 15 minutes before

void btnDown()
{
	if (!btnAState && !digitalRead(BTNA))
	{
		btnAState = true;
	}
	if (!btnBState && !digitalRead(BTNB))
	{
		btnBState = true;
	}

}

struct config_t {

	uint32_t alarm;
	bool alarmOn;
} config;

inline void incrementSetTime()
{
	if (mode == 1)
		setTime.setHour(setTime.hour() + 1);
	else if (mode == 2)
		setTime.setMinute(setTime.minute() + 1);
	else if (mode == 3)
		alarmTime.setHour(alarmTime.hour() + 1);
	else if (mode == 4)
		alarmTime.setMinute(alarmTime.minute() + 1);
	else if (mode == 5)
	{
		if (config.alarmOn) config.alarmOn = false;
		else config.alarmOn = true;
	}

	setTime.setSecond(0);
	alarmTime.setSecond(0);
}

void timer()
{
	if (btnAState && !digitalRead(BTNA))
	{
		btnADown++;
		if (mode == 0 && btnADown >= HOLDTIME)
		{
			btnADown = 0;
			btnAState = false;
			if (readingMode) readingMode = false;
			else readingMode = true;
		}
		else if (mode != 0 && btnADown >= SETHOLDTIME)
		{
			btnADown = 0;
			incrementSetTime();
		}
	}
	else if (btnAState  && digitalRead(BTNA))
	{
		if (btnADown >= PRESSTIME)
		{
			showTime = true;
			if (lightOn)
			{
				lightOn = false;
				dismissed = true;
			}
			if (readingMode)
			{
				readingMode = false;
			}
			timeOn = millis();

			if (!btnAHeld)
			{
				incrementSetTime();
			}
		}

		btnADown = 0;
		btnAState = false;
		btnAHeld = false;
	}

	if (btnBState && !digitalRead(BTNB))
	{
		btnBDown++;
		if (btnBDown >= HOLDTIME)
		{
			btnBHeld = true;
			btnBState = false;
			btnBDown = 0;
		}
	}
	else if (btnBState  && digitalRead(BTNB))
	{
		if (btnBDown >= PRESSTIME)
		{
			showTime = true;
			if (lightOn)
			{
				lightOn = false;
				dismissed = true;
			}
			if (readingMode)
			{
				readingMode = false;
			}
			timeOn = millis();
		}

		btnBDown = 0;
		btnBState = false;
	}

}

void setup()
{
	EEPROM_readAnything(0, config);
	alarmTime = DateTime(config.alarm);

	Serial.begin(115200);
	pinMode(LEDPIN, OUTPUT);

	Wire.begin();
	RTC.begin();
	disp.begin(0x70);
	disp.setBrightness(1);
	//dt_now = DateTime(__DATE__, __TIME__);
	//RTC.adjust(dt_now);

	pinMode(BTNA, INPUT);
	digitalWrite(BTNA, HIGH);
	attachInterrupt(0, btnDown, CHANGE);
	pinMode(BTNB, INPUT);
	digitalWrite(BTNB, HIGH);
	attachInterrupt(1, btnDown, CHANGE);

	Timer1.initialize(10000); //every 10ms
	Timer1.attachInterrupt(timer);

}

void printDateTime(DateTime time)
{
	Serial.print(time.hour()); Serial.print(":"); Serial.print(time.minute()); Serial.print(":"); Serial.println(time.second());
}

void loop()
{
	dt_now = RTC.now();

	if (mode == 0)
	{
		static uint8_t h, m, s;
		h = dt_now.hour();
		m = dt_now.minute();
		s = dt_now.second();

		disp.clear();
		if (showTime)
		{
			disp.writeDigitNum(0, h / 10, false);
			disp.writeDigitNum(1, h % 10, false);
			disp.drawColon(s % 2 == 0);
			disp.writeDigitNum(3, m / 10, false);
			disp.writeDigitNum(4, m % 10, false);
		}
		if (showTime && millis() - timeOn > 30000)
			showTime = false;

		static uint16_t sDay, sAlarm;
		static int32_t diff = 0;

		sDay = dt_now.secondInDay();
		sAlarm = alarmTime.secondInDay();
		if (sAlarm < ALARMSTART) sAlarm = sAlarm + 86400;
		
		diff = (uint32_t)sAlarm - (uint32_t)sDay;
		Serial.println(diff);

		if (config.alarmOn && !lightOn && !dismissed && diff <= ALARMSTART && diff > 0)
		{
			lightOn = true;
		}
		else if (diff > ALARMSTART || diff < 0)
		{
			dismissed = false;
		}

		if (lightOn)
		{
			if (diff < 0) diff = 0;

			analogWrite(LEDPIN, map(ALARMSTART - diff, 0, ALARMSTART, 0, 255));
		}
		else if (readingMode)
		{
			analogWrite(LEDPIN, 255);
		}
		else
		{
			analogWrite(LEDPIN, 0);
		}
	}
	else if (mode == 1) // set time hours
	{
		disp.clear();
		disp.drawColon(true);

		disp.writeDigitNum(0, setTime.hour() / 10, dt_now.second() % 2 == 0);
		disp.writeDigitNum(1, setTime.hour() % 10, false);

		disp.writeDigitNum(3, setTime.minute() / 10, false);
		disp.writeDigitNum(4, setTime.minute() % 10, false);
	}
	else if (mode == 2) //set time minutes
	{
		disp.clear();
		disp.drawColon(true);

		disp.writeDigitNum(0, setTime.hour() / 10, false);
		disp.writeDigitNum(1, setTime.hour() % 10, false);

		disp.writeDigitNum(3, setTime.minute() / 10, dt_now.second() % 2 == 0);
		disp.writeDigitNum(4, setTime.minute() % 10, false);
	}
	else if (mode == 3) // set alarm hours
	{
		disp.clear();
		disp.drawColon(true);

		disp.writeDigitNum(0, alarmTime.hour() / 10, false);
		disp.writeDigitNum(1, alarmTime.hour() % 10, dt_now.second() % 2 == 0);

		disp.writeDigitNum(3, alarmTime.minute() / 10, false);
		disp.writeDigitNum(4, alarmTime.minute() % 10, false);
	}
	else if (mode == 4) //set alarm minutes
	{
		disp.clear();
		disp.drawColon(true);

		disp.writeDigitNum(0, alarmTime.hour() / 10, false);
		disp.writeDigitNum(1, alarmTime.hour() % 10, false);

		disp.writeDigitNum(3, alarmTime.minute() / 10, false);
		disp.writeDigitNum(4, alarmTime.minute() % 10, dt_now.second() % 2 == 0);
	}
	else if (mode == 5)
	{
		disp.clear();
		if (config.alarmOn)
		{
			disp.writeDigitNum(1, 0, false);
			disp.writeDigitRaw(3, 0b01010100);
		}
		else
		{
			disp.writeDigitNum(1, 0, false);
			disp.writeDigitRaw(3, 0b01110001);
			disp.writeDigitRaw(4, 0b01110001);
		}
	}

	disp.writeDisplay();

	//if (btnAHeld)
	//{
	//	if (mode == 0)
	//	{
	//		btnAHeld = false;
	//		readingMode = true;
	//	}
	//}

	if (btnBHeld)
	{
		btnBHeld = false;
		mode++;

		if (mode >= 6)
			mode = 0;

		if (mode == 1)
			setTime = dt_now;

		if (mode == 3)
		{
			RTC.adjust(DateTime(0, 0, 0, setTime.hour(), setTime.minute()));
		}

		if (mode == 0)
		{
			config.alarm = alarmTime.unixtime();
			EEPROM_writeAnything(0, config);
		}
	}
}
