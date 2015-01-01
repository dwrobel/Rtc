


#include <avr/pgmspace.h>
#include <Wire.h>
#include "RtcUtility.h"
#include "RtcDS3231.h"


#if ARDUINO < 100
#define Write(x) send(x) 
#define Read(x) receive(x) 
#else
#define Write(x) write(static_cast<uint8_t>(x))
#define Read(x) read(x)
#endif

//I2C Slave Address  
#define DS3231_ADDRESS 0x68  

//DS3231 Register Addresses
#define DS3231_REG_TIMEDATE   0x00
#define DS3231_REG_ALARMONE   0x07
#define DS3231_REG_ALARMTWO   0x0B

#define DS3231_REG_CONTROL    0x0E
#define DS3231_REG_STATUS     0x0F
#define DS3231_REG_AGING      0x10

#define DS3231_REG_TEMP       0x11


// DS3231 Control Register Bits
#define DS3231_A1IE  0
#define DS3231_A2IE  1
#define DS3231_INTCN 2
#define DS3231_RS1   3
#define DS3231_RS2   4
#define DS3231_CONV  5
#define DS3231_BBSQW 6
#define DS3231_EOSC  7
#define DS3231_AIEMASK (_BV(DS3231_A1IE) | _BV(DS3231_A2IE))
#define DS3231_RSMASK (_BV(DS3231_RS1) | _BV(DS3231_RS2))

// DS3231 Status Register Bits
#define DS3231_A1F      0
#define DS3231_A2F      1
#define DS3231_BSY      2
#define DS3231_EN32KHZ  3
#define DS3231_OSF      7
#define DS3231_AIFMASK (_BV(DS3231_A1F) | _BV(DS3231_A2F))

void RtcDS3231::Begin()
{
    Wire.begin();
}

bool RtcDS3231::IsDateTimeValid()
{
    uint8_t status = getReg(DS3231_REG_STATUS);
    return !(status & _BV(DS3231_OSF));
}

bool RtcDS3231::GetIsRunning()
{
    uint8_t creg = getReg(DS3231_REG_CONTROL);
    return !(creg & _BV(DS3231_EOSC));
}

void RtcDS3231::SetIsRunning(bool isRunning)
{
    uint8_t creg = getReg(DS3231_REG_CONTROL);
    if (isRunning)
    {
        creg &= ~_BV(DS3231_EOSC);
    }
    else
    {
        creg |= _BV(DS3231_EOSC);
    }
    setReg(DS3231_REG_CONTROL, creg);
}

void RtcDS3231::SetDateTime(const RtcDateTime& dt)
{
    // clear the invalid flag
    uint8_t status = getReg(DS3231_REG_STATUS);
    status &= ~_BV(DS3231_OSF); // clear the flag
    setReg(DS3231_REG_STATUS, status);

	// set the date time
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.Write(DS3231_REG_TIMEDATE);           

    Wire.Write(Uint8ToBcd(dt.Second()));
    Wire.Write(Uint8ToBcd(dt.Minute()));
    Wire.Write(Uint8ToBcd(dt.Hour())); // 24 hour mode only

    Wire.Write(Uint8ToBcd(dt.DayOfWeek()));
    Wire.Write(Uint8ToBcd(dt.Day()));
    Wire.Write(Uint8ToBcd(dt.Month()));
    Wire.Write(Uint8ToBcd(dt.Year() - 2000));

    Wire.endTransmission();
}

uint8_t BcdToBin24Hour(uint8_t bcdHour)
{
    uint8_t hour;
    if (bcdHour & _BV(6))
    {
        // 12 hour mode, convert to 24
        bool isPm = ((bcdHour & _BV(5)) != 0);

        hour = BcdToUint8(bcdHour & 0x1f);
        if (isPm)
        {
           hour += 12;
        }
    }
    else
    {
        hour = BcdToUint8(bcdHour);
    }
    return hour;
}

RtcDateTime RtcDS3231::GetDateTime()
{
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.Write(DS3231_REG_TIMEDATE);           
    Wire.endTransmission();

    Wire.requestFrom(DS3231_ADDRESS, 7);
    uint8_t second = BcdToUint8(Wire.Read() & 0x7F);
    uint8_t minute = BcdToUint8(Wire.Read());
    uint8_t hour = BcdToBin24Hour(Wire.Read());

    Wire.Read();  // throwing away day of week as we calculate it

    uint8_t dayOfMonth = BcdToUint8(Wire.Read());
    uint8_t monthRaw = Wire.Read();
    uint16_t year = BcdToUint8(Wire.Read()) + 2000;

    if (monthRaw & _BV(7)) // century wrap flag
    {
        year += 100;
    }
    uint8_t month = BcdToUint8(monthRaw & 0x7f);
    

    return RtcDateTime(year, month, dayOfMonth, hour, minute, second);
}

RtcTemperature RtcDS3231::GetTemperature()
{
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.Write(DS3231_REG_TEMP);          
    Wire.endTransmission();

	Wire.requestFrom(DS3231_ADDRESS, 2);
    int8_t degrees = Wire.Read();               
    // fraction is just the upper bits
    // representing 1/4 of a degree
    uint8_t fract = (Wire.Read() >> 6) * 25;    
    
    return RtcTemperature(degrees, fract);
}

void RtcDS3231::Enable32kHzPin(bool enable)
{
	uint8_t sreg = getReg(DS3231_REG_STATUS);    

	if (enable == true) 
    {
		sreg |=  _BV(DS3231_EN32KHZ); 
	} 
    else 
    {
		sreg &= ~_BV(DS3231_EN32KHZ); 
	}

	setReg(DS3231_REG_STATUS, sreg);
}

void RtcDS3231::SetSquareWavePin(DS3231SquareWavePinMode pinMode)
{
    uint8_t creg = getReg(DS3231_REG_CONTROL);
    
    // clear all relevant bits to a known "off" state
    creg &= ~(DS3231_AIEMASK | _BV(DS3231_BBSQW));
    creg |= _BV(DS3231_INTCN);  // set INTCN to disables SQW

    switch (pinMode)
    {
    case DS3231SquareWavePin_ModeNone:
        break;

    case DS3231SquareWavePin_ModeBatteryBackup:
        creg |= _BV(DS3231_BBSQW); // set battery backup flag
        creg &= ~_BV(DS3231_INTCN); // clear INTCN to enable SQW 
        break;

    case DS3231SquareWavePin_ModeClock:
        creg &= ~_BV(DS3231_INTCN); // clear INTCN to enable SQW 
        break;

    case DS3231SquareWavePin_ModeAlarmOne:
        creg |= _BV(DS3231_A1IE);
        break;

    case DS3231SquareWavePin_ModeAlarmTwo:
        creg |= _BV(DS3231_A2IE);
        break;

    case DS3231SquareWavePin_ModeAlarmBoth:
        creg |= _BV(DS3231_A1IE) | _BV(DS3231_A2IE);
        break;
    }
        
    setReg(DS3231_REG_CONTROL, creg);
}

void RtcDS3231::SetSquareWavePinClockFrequency(DS3231SquareWaveClock freq)
{
	uint8_t creg = getReg(DS3231_REG_CONTROL);

	creg &= ~DS3231_RSMASK; // Set to 0
	creg |= (freq & DS3231_RSMASK); // Set freq bits

    setReg(DS3231_REG_CONTROL, creg);
}

void RtcDS3231::SetAlarmOne(const DS3231AlarmOne& alarm)
{
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.Write(DS3231_REG_ALARMONE);           

    Wire.Write(Uint8ToBcd(alarm.Second()) | ((alarm.ControlFlags() & 0x01) << 7));
    Wire.Write(Uint8ToBcd(alarm.Minute()) | ((alarm.ControlFlags() & 0x02) << 6));
    Wire.Write(Uint8ToBcd(alarm.Hour()) | ((alarm.ControlFlags() & 0x04) << 5)); // 24 hour mode only

    Wire.Write(Uint8ToBcd(alarm.DayOf()) | ((alarm.ControlFlags() & 0x18) << 3));

    Wire.endTransmission();
}

void RtcDS3231::SetAlarmTwo(const DS3231AlarmTwo& alarm)
{
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.Write(DS3231_REG_ALARMTWO);           

    Wire.Write(Uint8ToBcd(alarm.Minute()) | ((alarm.ControlFlags() & 0x01) << 7));
    Wire.Write(Uint8ToBcd(alarm.Hour()) | ((alarm.ControlFlags() & 0x02) << 6)); // 24 hour mode only

    Wire.Write(Uint8ToBcd(alarm.DayOf()) | ((alarm.ControlFlags() & 0x0c) << 4));

    Wire.endTransmission();
}

DS3231AlarmOne RtcDS3231::GetAlarmOne()
{
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.Write(DS3231_REG_ALARMONE);           
    Wire.endTransmission();

    Wire.requestFrom(DS3231_ADDRESS, 4);

    uint8_t raw = Wire.Read();
    uint8_t flags = (raw & 0x80) >> 7;
    uint8_t second = BcdToUint8(raw & 0x7F);

    raw = Wire.Read();
    flags |= (raw & 0x80) >> 6;
    uint8_t minute = BcdToUint8(raw & 0x7F);

    raw = Wire.Read();
    flags |= (raw & 0x80) >> 5;
    uint8_t hour = BcdToBin24Hour(raw & 0x7f);

    raw = Wire.Read();
    flags |= (raw & 0xc0) >> 3;
    uint8_t dayOf = BcdToUint8(raw & 0x3f);

    return DS3231AlarmOne(dayOf, hour, minute, second, (DS3231AlarmOneControl)flags);
}

DS3231AlarmTwo RtcDS3231::GetAlarmTwo()
{
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.Write(DS3231_REG_ALARMTWO);           
    Wire.endTransmission();

    Wire.requestFrom(DS3231_ADDRESS, 3);

    uint8_t raw = Wire.Read();
    uint8_t flags = (raw & 0x80) >> 7;
    uint8_t minute = BcdToUint8(raw & 0x7F);

    raw = Wire.Read();
    flags |= (raw & 0x80) >> 6;
    uint8_t hour = BcdToBin24Hour(raw & 0x7f);

    raw = Wire.Read();
    flags |= (raw & 0xc0) >> 4;
    uint8_t dayOf = BcdToUint8(raw & 0x3f);

    return DS3231AlarmTwo(dayOf, hour, minute, (DS3231AlarmTwoControl)flags);
}

DS3231AlarmFlag RtcDS3231::LatchAlarmsTriggeredFlags()
{
  	uint8_t sreg = getReg(DS3231_REG_STATUS);  
    uint8_t alarmFlags = (sreg & DS3231_AIFMASK);
    sreg &= ~DS3231_AIFMASK; // clear the flags
    setReg(DS3231_REG_STATUS, sreg);
    return (DS3231AlarmFlag)alarmFlags;
}

void RtcDS3231::ForceTemperatureCompensationUpdate(bool block)
{
	uint8_t creg = getReg(DS3231_REG_CONTROL); 
	creg |= _BV(DS3231_CONV); // Write CONV bit
    setReg(DS3231_REG_CONTROL, creg);

	while (block && (creg & _BV(DS3231_CONV)) != 0)
	{
		// Block until CONV is 0
		creg = getReg(DS3231_REG_CONTROL); 
	} 
}

int8_t RtcDS3231::GetAgingOffset()
{
    return getReg(DS3231_REG_AGING);
}

void RtcDS3231::SetAgingOffset(int8_t value)
{
    setReg(DS3231_REG_AGING, value);
}

uint8_t RtcDS3231::getReg(uint8_t regAddress)
{
    Wire.beginTransmission(DS3231_ADDRESS);
	Wire.Write(regAddress);
	Wire.endTransmission();

	// control register
	Wire.requestFrom(DS3231_ADDRESS, 1);

	uint8_t regValue = Wire.Read();     
    return regValue;
}

void RtcDS3231::setReg(uint8_t regAddress, uint8_t regValue)
{
    Wire.beginTransmission(DS3231_ADDRESS);
	Wire.Write(regAddress);
	Wire.Write(regValue);
	Wire.endTransmission();
}