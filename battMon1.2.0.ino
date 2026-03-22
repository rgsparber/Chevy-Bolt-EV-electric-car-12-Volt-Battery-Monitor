//#include <avr/sleep.h> //is this needed?

/*************************************************
; ************************************************
;     EV 12V Battery Monitor                     *
;     Version 1.0.0     3/3/2026                 * 
;    							                 *
; This work is licensed under the Creative       *
; Commons Attribution 4.0 International License. *
; To view a copy of this license, visit          *
; http://creativecommons.org/licenses/by/4.0/    *
; or send a letter to Creative Commons,          *
; PO Box 1866, Mountain View, CA 94042, USA.     *
; *********** by R.G. Sparber*********************
; ************************************************
;                Arduino IDE settings
;	Board: ATtiny Microcontrollers	ATtiny 25/45/85
;	Processor ATtiny 85
;	clock: Internal 8 MHz
;
; ============================================
;   A T T I N Y - 8 5  H A R D W A R E   I N F O R M A T I O N   
; ============================================
;   >>>clock must be set to internal 8 MHz<<<
; physical pin  logical pin name  use
;       1         PB5       Reset (active low)
;       2         PB3       ADC3  analog input
;       3         PB4		calibration  (active low) 
;       4         GND       ground 
;       5         PB0       output
;       6         PB1		unused
;       7         PB2		unused
;       8         VCC       Vcc also used for Vref
; ============================================
;      P O R T S   A N D   L O G I C A L  P I N  N A M E S 
; =====================================
*********************************************************************/

const byte analogInByte = 3;//ADC3
const byte outputByte = 0;//PB0
const byte LEDbrightnessByte = 0;//port that controls the LED intensity
const byte ADCport_Byte = 3;//port that accepts an analog voltage read by the ADC
const byte temperaturePort_Byte = 4;//ADC4 ties to the internal temperature sensor.
const byte calibrationEnableByte = 4;//port that connects to the calibration button
const byte notRESET_byte = 5;//port that connects to the reset button
/***************LED Intensity**************/ 
const byte brighterByte = 1;//make the LED brighter
const byte dimmerByte = 0;//make the LED dimmer
/***************ADC and MUX Configurations**************/ 
const byte ADMUX_OperationalConfigurationByte = 0b00000011;//Use Vcc as Vref (bit 7 = 0, bit 6 = 0, bit 4 = 0), ADC output right adjusted (bit 5 = 0). The right 4 bits are MUX[3:0] = 0011: ADC3 is single ended input. This works in conjunction with analogRead(ADCport_Byte)
const byte ADMUX_TemperatureConfigurationByte = 0b10001111;//Use 1.1V as Vref (bit 7 = 1, bit 6 = 0, bit 4 = 0), ADC output right adjusted (bit 5 = 0). The right 4 bits are MUX[3:0] = 1111: ADC4 is temperature sensor input.
/*********************************************************/

/* ============================================
;           GLOBAL VARIABLES AND CONSTANTS
; =============================================*/
int ADC_OutputInt = 0;
int batteryThresholdCountInt = 0;
int temperatureCountAtCalibrationInt = 0;

const bool exerciseLED_bool = true;//used by alarm() to flash the LED for 1 second and then return
const bool realBool = false;//used by alarm() to flash the LED until reset is pressed
/*============================================
					EEPROM MAP
  ============================================
	ADDRESS		DESC
		0		threshold LSB
		1		threshold MSB		batteryThresholdCountInt
		2		temperature LSB
		3		temperature MSB		temperatureCountAtCalibrationInt		
  ============================================*/
const byte batteryThresholdAddressLSB_Byte = 0;
const byte batteryThresholdAddressMSB_Byte = 1;
const byte temperatureCountAtThresholdAddressLSB_Byte = 2;
const byte temperatureCountAtThresholdAddressMSB_Byte = 3;
/*============================================*/
  
void setup()
{
	//define GPIO
	pinMode(notRESET_byte,INPUT_PULLUP);//RESET active low [PB5 is physical pin 1]
	pinMode(calibrationEnableByte,INPUT_PULLUP);//pull low to go to calibration mode when 12.000V applied to Vbattery [PB4 is physical pin 3]
	
	pinMode(ADCport_Byte,INPUT);//ADC3 [PB3 is physical pin 2]

	pinMode(LEDbrightnessByte,OUTPUT);//Ties to R5 to increase LED brightness when high [PB0 is physical pin 5]
    /*********************************************************
	P O R T,  A M P L I F I E R,  &  A D C  C O N F I G U R A T I O N
    *********************************************************/
	ADMUX = ADMUX_OperationalConfigurationByte;
	delay(2);//give time for ADC and MUX to settle down
	/*********************************************************
				SYSTEM CLOCK PRESCALLER
	*********************************************************/
    CLKPR = 0b10000000;//this ensures that the system clock prescaller is at x1. During development it somehow was changed to 8 and the measured delay was 8 times slower than the specified delay.
    CLKPR = 0;//the value of CLKPR set above is locked in when bit 7 is set to 0
	delay(2);//give time for prescaller to settle down
	/*********************************************************/
	
	//retrieve battery threshold
	byte batteryThresholdMSB_Byte = EEPROM_Read(batteryThresholdAddressMSB_Byte);
	byte batteryThresholdLSB_Byte = EEPROM_Read(batteryThresholdAddressLSB_Byte);
	batteryThresholdCountInt = batteryThresholdMSB_Byte <<8 | batteryThresholdLSB_Byte;
	
	//retrieve temperature during calibration
	byte temperatureCountAtCalibrationLSB_Byte = EEPROM_Read(temperatureCountAtThresholdAddressLSB_Byte);
	byte temperatureCountAtCalibrationMSB_Byte = EEPROM_Read(temperatureCountAtThresholdAddressMSB_Byte);
	temperatureCountAtCalibrationInt = temperatureCountAtCalibrationMSB_Byte <<8 | temperatureCountAtCalibrationLSB_Byte;
	
	alarm(exerciseLED_bool);//brighten the LED for 1/4 second and then return to dimm. Then proceed to monitoring the 12V battery.		
}//end of setup()
 
void loop()
{
	calibrationQ();//if calibration requested, battery must be at the threshold which should be 12.000V.
	checkThreshold();//if battery voltage represented by a count is less than the threshold, verify it is consistent. If it is, set alarm and wait for reset or calibration buttons to be pushed			
}//end of loop( )

void checkThreshold()
{
	if(analogRead(ADCport_Byte) < temperatureCorrectedBatteryThreshold())verifyAndAct();
}

void verifyAndAct()
{//read scaled battery voltage 5 times with 10 ms between readings and vote to see if it is below the threshold set during the last calibration most of the time
	byte failureCountByte = 0;
	for(byte testByte = 0;testByte < 5;testByte++)
	{
		if(analogRead(ADCport_Byte) < temperatureCorrectedBatteryThreshold())failureCountByte++;
		delay(10);
	}
	if (failureCountByte > 3)alarm(realBool);//majority of reads were threshold so alarm and don't come back. We escape this state via reset or new calibration.
	//if majority are not below threshold, return
}

void alarm(bool typeBool)
{//if passed parameter is "real" [= false], we continuously flicker at a rate of interval Ms on, interval Ms off until RESET or calibration is pressed.
//if passed parameter is "exerciseLED" [= true], we flicker once and return.
	unsigned long startTimeULong = millis();
	byte intervalMsByte = 250;
	while(1)
	{
		digitalWrite(LEDbrightnessByte,brighterByte);
		delay(intervalMsByte);		
		digitalWrite(LEDbrightnessByte,dimmerByte);
		delay(intervalMsByte);	
		if (typeBool)return;//I leave with the LED dim		
		if(!digitalRead(calibrationEnableByte))return;//pressing the calibration button breaks us free from alarm. I leave with the LED dim.
	}
}

void EEPROM_Write(byte AddressByte, byte DataByte)
{
    if(EEPROM_Read(AddressByte) == DataByte)return;//if the write doesn't change the data, just return
    while(EECR & (1<<EEPE));// Wait for completion of previous write
    EECR = (0<<EEPM1)|(0<<EEPM0);// Set Programming mode to Erase and Write in one operation by setting EEPM1 to 0 and EEPM0 to 0. See table 5-1.
    EEARH = 0;//EE address register, high byte is 1 bit which I can leave at 0
    EEARL = AddressByte;// Set up low address byte
    EEDR = DataByte;// populate data register
    EECR |= (1<<EEMPE); // Write logical one to EEMPE.  EECR is the control register
    EECR |= (1<<EEPE);// Start eeprom write by setting EEPE
    if(EEPROM_Read(AddressByte) != DataByte)EEPROMwriteFailure();//if the write does not take, flicker LED until reset invoked
}

byte EEPROM_Read(byte AddressByte)//this code is based on the example in the ATtiny85 spec sheet, page 19
{
    while(EECR & (1<<EEPE));//Wait for completion of previous write
    EEARL = AddressByte;// Set up low address register
    EEARH = 0;//high address is always 0 for me 
    /* Start EEPROM read by writing EERE */
    EECR |= (1<<EERE);
    // Return data from EEDR data register.
    return EEDR;
}    
	
void EEPROMwriteFailure()
{//If the write to EEPROM fails readback, flicker LED between dim and bright with flickerTime Ms in each state until RESET is pressed
	byte flickerTimeMsByte = 50;
	while(1)
	{
		digitalWrite(LEDbrightnessByte,brighterByte);//increase LED current from 6 to 18 mA
		delay(flickerTimeMsByte);
		digitalWrite(LEDbrightnessByte,dimmerByte);//decrease LED current from 18 mA to 6
		delay(flickerTimeMsByte);
	}
}

void calibrationQ()
{
/***********************************************************************************************************
First set the battery voltage to the trip value, typically 12.000 volts. When the calibration button is pressed, we read the battery voltage 10 times and take the average which is then our threshold. We also flash the LED on for 0.1 seconds and off for 0.1 seconds during each reading. Then we save the threshold to EEPROM where is can be used if power is lost. 

When the LED stops flashing, raise the battery voltage to the normal level, typically 12.7V. Then press reset.

This strategy nulls out the error in the voltage divider and the voltage reference. It does not address any long term drift or a change due to temperature. How initial analysis says the resistor ratios should not be sensative to temperature if all values change by the same percentage for a given temperature change. 
***********************************************************************************************************/
	if(!digitalRead(calibrationEnableByte))//see if calibration button pushed
	{//we are in calibration mode with the applied battery as our threshold.
		int readSumInt = 0;
		//calculate the average of 10 readings of the scaled battery voltage to integrate out any noise
		for(byte readCountByte = 0; readCountByte < 10; readCountByte++)
		{
			readSumInt = readSumInt + analogRead(ADCport_Byte);
			delay(10);//spread out the readings a little
		}
		batteryThresholdCountInt = readSumInt/10;//take the average
		EEPROM_Write(batteryThresholdAddressLSB_Byte,lowByte(batteryThresholdCountInt));//save LSB of the threshold in battery Threshold Address LSB
		EEPROM_Write(batteryThresholdAddressMSB_Byte,highByte(batteryThresholdCountInt));//save MSB of the threshold in battery Threshold Address MSB
		//record temperature count
		ADMUX = ADMUX_TemperatureConfigurationByte;//Use internal 1.1V reference
		delay(2);//give time for ADC and MUX to settle down
		temperatureCountAtCalibrationInt = analogRead(temperaturePort_Byte);//read built in temperature sensor
		ADMUX = ADMUX_OperationalConfigurationByte;// Change back to original state of ADC and MUX
		delay(2);//give time for ADC and MUX to settle down
		EEPROM_Write(temperatureCountAtThresholdAddressLSB_Byte,lowByte(temperatureCountAtCalibrationInt));//save LSB of temperature
		EEPROM_Write(temperatureCountAtThresholdAddressMSB_Byte,highByte(temperatureCountAtCalibrationInt));//save MSB of temperature
		digitalWrite(LEDbrightnessByte,brighterByte);//indicate calibration is over with 1/2 second brightening
		delay(500);		
		digitalWrite(LEDbrightnessByte,dimmerByte);
		while(1);//wait until reset signal is applied [ground on pin 1]
	}
}

int temperatureCorrectedBatteryThreshold()
{
	//threshold was recorded at the recorded temperature and must be adjusted for the ambient
	//prepare to read ambient temperature
	ADMUX = ADMUX_TemperatureConfigurationByte;//Use internal 1.1V reference
	delay(2);//give time for ADC and MUX to settle down
	int temperatureCountAtLiveReadingInt = analogRead(temperaturePort_Byte);//Read ambient temp. This port does not appear as an external pin.
	ADMUX = ADMUX_OperationalConfigurationByte;//Restore ADC ref to original state: use Vcc as Vref
	delay(2);//give time for ADC and MUX to settle down
	//get temperature when calibration was performed
	byte temperatureCountAtCalibrationLSB_Byte = EEPROM_Read(temperatureCountAtThresholdAddressLSB_Byte);
	byte temperatureCountAtCalibrationMSB_Byte = EEPROM_Read(temperatureCountAtThresholdAddressMSB_Byte);
	temperatureCountAtCalibrationInt = (temperatureCountAtCalibrationMSB_Byte <<8 | temperatureCountAtCalibrationLSB_Byte);//assemble the temperature measured during calibration.
	float deltaTemperatureCountFloat = float(temperatureCountAtLiveReadingInt - temperatureCountAtCalibrationInt);
	int temperatureCorrectedBatteryThresholCountdInt = batteryThresholdCountInt + (deltaTemperatureCountFloat/(-2.6));//-2.6 is based on a test using my heat gun and non-contact thermometer. As the heat of the board rose, I monitored the the threshold. It rose. Therefore, as temperature rises, I need to reduce the threshold to get it back to the calibration value.
	return temperatureCorrectedBatteryThresholCountdInt;
}