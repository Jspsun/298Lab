#include "main.h"
#include "driverlib/driverlib.h"
#include <stdlib.h>
#include "hal_LCD.h"
#include <stdio.h>
#include <stdbool.h>

#define NUM_SAMPLES_TO_AVERAGE 10 //TODO: tweak if needed
#define lightDex 4
#define tempZoneOneDex 1
#define tempZoneTwoDex 3
#define moistureZoneOneDex 2
#define moistureZoneTwoDex 0

#define zone1 0
#define zone2 1

char ADCState = 0; //Busy state of the ADC
int16_t ADCResult = 0; //Storage for the ADC conversion result
int NUM_SENSORS = 5;
int zoneToDisplay = 0;
enum Servo servo;
unsigned int isOver;

int irrigationThresh=10;
int ventThresh=22;
int lightVal;

typedef struct {
    int16_t light[NUM_SAMPLES_TO_AVERAGE];
    int16_t tempZoneOne[NUM_SAMPLES_TO_AVERAGE];
    int16_t tempZoneTwo[NUM_SAMPLES_TO_AVERAGE];
    int16_t moistureZoneOne[NUM_SAMPLES_TO_AVERAGE];
    int16_t moistureZoneTwo[NUM_SAMPLES_TO_AVERAGE];
} Values;

int setMux(int n);
int storeSensorReadings(int sensor, int i);
int getAverageSensorReading(int sensor);
void initAll(void);
int isDaytime();
void zone_select(void);
void displayLCD(_Bool zone, unsigned char *temp, unsigned char *soil);
void set_led_ind(int selected, int state);

volatile static _Bool buttonPress = false;                  /* zone select button state */
static _Bool zone = zone1;                                  /* zone 1 or zone 2 */
static uint8_t temp[2] = {0, 0};                            /* temperature values */
static uint8_t soil[2] = {0, 0};                            /* soil values */

Values *averagedValues;
int lightDexData = 0;
int tempZoneOneDexData = 0;
int tempZoneTwoDexData = 0;
int moistureZoneOneDexData = 0;
int moistureZoneTwoDexData = 0;

int ventZone1;
int ventZone2;
int irrZone1;
int irrZone2;

void main(void){
    initAll();

    servo = irri1;
    averagedValues = malloc(sizeof(Values));

    int q;
    for (q = 0; q < NUM_SAMPLES_TO_AVERAGE; q++){
        averagedValues->light[q]=9;
        averagedValues->tempZoneOne[q]=9;
        averagedValues->tempZoneTwo[q]=9;
        averagedValues->moistureZoneOne[q]=9;
        averagedValues->moistureZoneTwo[q]=9;
    }

    // preloop to get some average values
    int i;
    for (i = 0; i < NUM_SAMPLES_TO_AVERAGE; i++){
        __delay_cycles(10000);
        int sensor = NUM_SENSORS;
        for (sensor = 0; sensor < NUM_SENSORS; sensor++){
            setMux(sensor);
            __delay_cycles(10000);
            storeSensorReadings(sensor, i);
            __delay_cycles(10000);
        }
    }
    int counter = -1;
    setMux(0);
    while (1){

        _Bool isDay = isDaytime();

        //update sensor readings -----------------------------------
        counter +=1;
        int sensor = (counter%NUM_SENSORS);
        int indexOfData;
        setMux(sensor);
        __delay_cycles(10000);

        if(sensor==lightDex){
            lightDexData = ((lightDexData +1) % NUM_SAMPLES_TO_AVERAGE);
            indexOfData = lightDexData;
        }
        else if(sensor==tempZoneOneDex){
            tempZoneOneDexData = ((tempZoneOneDexData +1) % NUM_SAMPLES_TO_AVERAGE);
            indexOfData = tempZoneOneDexData;
        }
        else if(sensor==tempZoneTwoDex){
            tempZoneTwoDexData = ((tempZoneTwoDexData +1) % NUM_SAMPLES_TO_AVERAGE);
            indexOfData = tempZoneTwoDexData;
        }
        else if(sensor==moistureZoneOneDex){
            moistureZoneOneDexData = ((moistureZoneOneDexData +1) % NUM_SAMPLES_TO_AVERAGE);
            indexOfData = moistureZoneOneDexData;
        }
        else if(sensor==moistureZoneTwoDex){
            moistureZoneTwoDexData = ((moistureZoneTwoDexData +1) % NUM_SAMPLES_TO_AVERAGE);
            indexOfData = moistureZoneTwoDexData;
        }

        storeSensorReadings(sensor, indexOfData);
        __delay_cycles(100000);
        if (counter >10000){
            counter = 0;
        }
        // -----------------------------------------------------------
        // show zones
        zone_select();

        temp[0]= getAverageSensorReading(tempZoneOneDex);
        temp[1]= getAverageSensorReading(tempZoneTwoDex);
        soil[0]= getAverageSensorReading(moistureZoneOneDex);
        soil[1]= getAverageSensorReading(moistureZoneTwoDex);
        displayLCD(zone, temp, soil);           /* display results */
        // -----------------------------------------------------------
        // deal with leds

        if(isDay){ // ventilation
            ventZone1 = (getAverageSensorReading(tempZoneOneDex) > ventThresh);
            ventZone2 = (getAverageSensorReading(tempZoneTwoDex) > ventThresh);
        } else { // irrigation
            irrZone1= (getAverageSensorReading(moistureZoneOneDex)<irrigationThresh);
            irrZone2= (getAverageSensorReading(moistureZoneTwoDex)<irrigationThresh);
        }
        set_led_ind(irri1, irrZone1 && !isDay);

//        if(irrZone1){
//            select_pwm_target(irri1);
//            output_pwm_on();
//            __delay_cycles(100000);
//        }

        set_led_ind(irri2, irrZone2 && !isDay);
        set_led_ind(vent1, ventZone1 && isDay);
        set_led_ind(vent2, ventZone2 && isDay);
        //--------------------------------------

    }
}
void Init_GPIO(void)
{
    // Set all GPIO pins to output low to prevent floating input and reduce power consumption
    GPIO_setOutputLowOnPin(
            GPIO_PORT_P1,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin(
            GPIO_PORT_P2,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin(
            GPIO_PORT_P3,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin(
            GPIO_PORT_P4,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin(
            GPIO_PORT_P5,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin(
            GPIO_PORT_P6,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin(
            GPIO_PORT_P7,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin(
            GPIO_PORT_P8,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);

    GPIO_setAsOutputPin(
            GPIO_PORT_P1,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setAsOutputPin(
            GPIO_PORT_P2,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setAsOutputPin(
            GPIO_PORT_P3,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setAsOutputPin(
            GPIO_PORT_P4,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setAsOutputPin(
            GPIO_PORT_P5,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setAsOutputPin(
            GPIO_PORT_P6,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setAsOutputPin(
            GPIO_PORT_P7,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setAsOutputPin(
            GPIO_PORT_P8,
            GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4
                    | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);

    //Set LaunchPad switches as inputs - they are active low, meaning '1' until pressed
    GPIO_setAsInputPinWithPullUpResistor(SW1_PORT, SW1_PIN);
    GPIO_setAsInputPinWithPullUpResistor(SW2_PORT, SW2_PIN);

    //Set LED1 and LED2 as outputs
    //GPIO_setAsOutputPin(LED1_PORT, LED1_PIN); //Comment if using UART
    GPIO_setAsOutputPin(LED2_PORT, LED2_PIN);
}

/* Clock System Initialization */
void Init_Clock(void)
{
    /*
     * The MSP430 has a number of different on-chip clocks. You can read about it in
     * the section of the Family User Guide regarding the Clock System ('cs.h' in the
     * driverlib).
     */

    /*
     * On the LaunchPad, there is a 32.768 kHz crystal oscillator used as a
     * Real Time Clock (RTC). It is a quartz crystal connected to a circuit that
     * resonates it. Since the frequency is a power of two, you can use the signal
     * to drive a counter, and you know that the bits represent binary fractions
     * of one second. You can then have the RTC module throw an interrupt based
     * on a 'real time'. E.g., you could have your system sleep until every
     * 100 ms when it wakes up and checks the status of a sensor. Or, you could
     * sample the ADC once per second.
     */
    //Set P4.1 and P4.2 as Primary Module Function Input, XT_LF
    GPIO_setAsPeripheralModuleFunctionInputPin(
    GPIO_PORT_P4,
                                               GPIO_PIN1 + GPIO_PIN2,
                                               GPIO_PRIMARY_MODULE_FUNCTION);

    // Set external clock frequency to 32.768 KHz
    CS_setExternalClockSource(32768);
    // Set ACLK = XT1
    CS_initClockSignal(CS_ACLK, CS_XT1CLK_SELECT, CS_CLOCK_DIVIDER_1);
    // Initializes the XT1 crystal oscillator
    CS_turnOnXT1LF(CS_XT1_DRIVE_1);
    // Set SMCLK = DCO with frequency divider of 1
    CS_initClockSignal(CS_SMCLK, CS_DCOCLKDIV_SELECT, CS_CLOCK_DIVIDER_1);
    // Set MCLK = DCO with frequency divider of 1
    CS_initClockSignal(CS_MCLK, CS_DCOCLKDIV_SELECT, CS_CLOCK_DIVIDER_1);
}

/* UART Initialization */
void Init_UART(void)
{
    /* UART: It configures P1.0 and P1.1 to be connected internally to the
     * eSCSI module, which is a serial communications module, and places it
     * in UART mode. This let's you communicate with the PC via a software
     * COM port over the USB cable. You can use a console program, like PuTTY,
     * to type to your LaunchPad. The code in this sample just echos back
     * whatever character was received.
     */

    //Configure UART pins, which maps them to a COM port over the USB cable
    //Set P1.0 and P1.1 as Secondary Module Function Input.
    GPIO_setAsPeripheralModuleFunctionInputPin(
    GPIO_PORT_P1,
                                               GPIO_PIN1,
                                               GPIO_PRIMARY_MODULE_FUNCTION);
    GPIO_setAsPeripheralModuleFunctionOutputPin(
    GPIO_PORT_P1,
                                                GPIO_PIN0,
                                                GPIO_PRIMARY_MODULE_FUNCTION);

    /*
     * UART Configuration Parameter. These are the configuration parameters to
     * make the eUSCI A UART module to operate with a 9600 baud rate. These
     * values were calculated using the online calculator that TI provides at:
     * http://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/MSP430BaudRateConverter/index.html
     */

    //SMCLK = 1MHz, Baudrate = 9600
    //UCBRx = 6, UCBRFx = 8, UCBRSx = 17, UCOS16 = 1
    EUSCI_A_UART_initParam param = { 0 };
    param.selectClockSource = EUSCI_A_UART_CLOCKSOURCE_SMCLK;
    param.clockPrescalar = 6;
    param.firstModReg = 8;
    param.secondModReg = 17;
    param.parity = EUSCI_A_UART_NO_PARITY;
    param.msborLsbFirst = EUSCI_A_UART_LSB_FIRST;
    param.numberofStopBits = EUSCI_A_UART_ONE_STOP_BIT;
    param.uartMode = EUSCI_A_UART_MODE;
    param.overSampling = 1;

    if (STATUS_FAIL == EUSCI_A_UART_init(EUSCI_A0_BASE, &param))
    {
        return;
    }

    EUSCI_A_UART_enable(EUSCI_A0_BASE);

    EUSCI_A_UART_clearInterrupt(EUSCI_A0_BASE,
    EUSCI_A_UART_RECEIVE_INTERRUPT);

    // Enable EUSCI_A0 RX interrupt
    EUSCI_A_UART_enableInterrupt(EUSCI_A0_BASE,
    EUSCI_A_UART_RECEIVE_INTERRUPT);
}

/* EUSCI A0 UART ISR - Echoes data back to PC host */
#pragma vector=USCI_A0_VECTOR
__interrupt
void EUSCIA0_ISR(void)
{
    uint8_t RxStatus = EUSCI_A_UART_getInterruptStatus(
            EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG);

    EUSCI_A_UART_clearInterrupt(EUSCI_A0_BASE, RxStatus);

    if (RxStatus)
    {
        EUSCI_A_UART_transmitData(EUSCI_A0_BASE,
                                  EUSCI_A_UART_receiveData(EUSCI_A0_BASE));
    }
}

/* PWM Initialization */
void Init_PWM(void)
{
    /*
     * The internal timers (TIMER_A) can auto-generate a PWM signal without needing to
     * flip an output bit every cycle in software. The catch is that it limits which
     * pins you can use to output the signal, whereas manually flipping an output bit
     * means it can be on any GPIO. This function populates a data structure that tells
     * the API to use the timer as a hardware-generated PWM source.
     *
     */
    //Generate PWM - Timer runs in Up-Down mode
    param.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
    param.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_1;
    param.timerPeriod = TIMER_A_PERIOD; //Defined in main.h
    param.compareRegister = TIMER_A_CAPTURECOMPARE_REGISTER_1;
    param.compareOutputMode = TIMER_A_OUTPUTMODE_RESET_SET;
    param.dutyCycle = HIGH_COUNT_OFF; //Defined in main.h

    //PWM_PORT PWM_PIN (defined in main.h) as PWM output
    GPIO_setAsPeripheralModuleFunctionOutputPin(
    PWM_PORT,
                                                PWM_PIN,
                                                GPIO_PRIMARY_MODULE_FUNCTION);
}

void Init_ADC(void)
{
    /*
     * To use the ADC, you need to tell a physical pin to be an analog input instead
     * of a GPIO, then you need to tell the ADC to use that analog input. Defined
     * these in main.h for A9 on P8.1.
     */

    //Set ADC_IN to input direction
    GPIO_setAsPeripheralModuleFunctionInputPin(
    ADC_IN_PORT,
                                               ADC_IN_PIN,
                                               GPIO_PRIMARY_MODULE_FUNCTION);

    //Initialize the ADC Module
    /*
     * Base Address for the ADC Module
     * Use internal ADC bit as sample/hold signal to start conversion
     * USE MODOSC 5MHZ Digital Oscillator as clock source
     * Use default clock divider of 1
     */
    ADC_init(ADC_BASE,
    ADC_SAMPLEHOLDSOURCE_SC,
             ADC_CLOCKSOURCE_ADCOSC,
             ADC_CLOCKDIVIDER_1);

    ADC_enable(ADC_BASE);

    /*
     * Base Address for the ADC Module
     * Sample/hold for 16 clock cycles
     * Do not enable Multiple Sampling
     */
    ADC_setupSamplingTimer(ADC_BASE,
    ADC_CYCLEHOLD_16_CYCLES,
                           ADC_MULTIPLESAMPLESDISABLE);

    //Configure Memory Buffer
    /*
     * Base Address for the ADC Module
     * Use input ADC_IN_CHANNEL
     * Use positive reference of AVcc
     * Use negative reference of AVss
     */
    ADC_configureMemory(ADC_BASE,
    ADC_IN_CHANNEL,
                        ADC_VREFPOS_AVCC,
                        ADC_VREFNEG_AVSS);

    ADC_clearInterrupt(ADC_BASE,
    ADC_COMPLETED_INTERRUPT);

    //Enable Memory Buffer interrupt
    ADC_enableInterrupt(ADC_BASE,
    ADC_COMPLETED_INTERRUPT);
}

//ADC interrupt service routine
#pragma vector=ADC_VECTOR
__interrupt
void ADC_ISR(void)
{
    uint8_t ADCStatus = ADC_getInterruptStatus(
    ADC_BASE,
                                               ADC_COMPLETED_INTERRUPT_FLAG);

    ADC_clearInterrupt(ADC_BASE, ADCStatus);

    if (ADCStatus)
    {
        ADCState = 0; //Not busy anymore
        ADCResult = ADC_getResults(ADC_BASE);
    }
}

void initAll(void){
    //Turn off interrupts during initialization
    __disable_interrupt();

    //Stop watchdog timer unless you plan on using it
    WDT_A_hold(WDT_A_BASE);

    // Initializations - see functions for more detail
    Init_GPIO();    //Sets all pins to output low as a default
    Init_PWM();     //Sets up a PWM output
    Init_ADC();     //Sets up the ADC to sample
    Init_Clock();   //Sets up the necessary system clocks
    Init_UART();    //Sets up an echo over a COM port
    Init_LCD();     //Sets up the LaunchPad LCD display

    PMM_unlockLPM5(); //Disable the GPIO power-on default high-impedance mode to activate previously configured port settings

    //All done initializations - turn interrupts back on.
    __enable_interrupt();
}

int setMux(int n)
{
    if (n > 7 || n < 0)
    {
        return -1;
    }
    // digits: P2.7;2.5;1.5
    if (n == 0)
    { //000
        GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN5);
        GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN5 | GPIO_PIN7);
    }
    else if (n == 1)
    { //001
        GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN5);
        GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN5 | GPIO_PIN7);
    }
    else if (n == 2)
    { //010
        GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN5);
        GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN7);
        GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN5);
    }
    else if (n == 3)
    { //011
        GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN5);
        GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN5);
        GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN7);
    }
    else if (n == 4)
    { //100
        GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN5);
        GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN5);
        GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN7);
    }
    else if (n == 5)
    { //101
//        GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN8);
//        GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN10 | GPIO_PIN5);
    }
    else if (n == 6)
    { //110
//        GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN10);
//        GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN5 | GPIO_PIN8);
    }
    else if (n == 7)
    { //111
        GPIO_setOutputHighOnPin(GPIO_PORT_P1,
        GPIO_PIN10 | GPIO_PIN8 | GPIO_PIN5);
    }
    return 0;
}

void output_pwm_off()
{
    param.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
    param.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_1;
    param.timerPeriod = TIMER_A_PERIOD; //Defined in main.h
    param.compareRegister = TIMER_A_CAPTURECOMPARE_REGISTER_1;
    param.compareOutputMode = TIMER_A_OUTPUTMODE_RESET_SET;
    param.dutyCycle = HIGH_COUNT_OFF; //Defined in main.h

    Timer_A_outputPWM(TIMER_A0_BASE, &param);
}

void output_pwm_on()
{
    param.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
    param.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_1;
    param.timerPeriod = TIMER_A_PERIOD; //Defined in main.h
    param.compareRegister = TIMER_A_CAPTURECOMPARE_REGISTER_1;
    param.compareOutputMode = TIMER_A_OUTPUTMODE_RESET_SET;
    param.dutyCycle = HIGH_COUNT_ON; //Defined in main.h

    Timer_A_outputPWM(TIMER_A0_BASE, &param);
}

void output_pwm_idle()
{
    param.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
    param.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_1;
    param.timerPeriod = TIMER_A_PERIOD; //Defined in main.h
    param.compareRegister = TIMER_A_CAPTURECOMPARE_REGISTER_1;
    param.compareOutputMode = TIMER_A_OUTPUTMODE_RESET_SET;
    param.dutyCycle = 0; //Defined in main.h

    Timer_A_outputPWM(TIMER_A0_BASE, &param);
}

void select_pwm_target(int selected)
{
    switch (selected)
    {
    case irri1:
        GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN6);
        GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN0);
        break;
    case irri2:
        GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN6);
        GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN0);
        break;
    case vent1:
        GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN6);
        GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN0);
        break;
    case vent2:
        GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN6);
        GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN0);
        break;
    }
    return;
}

void read_adc(){
    if (ADCState == 0) {
        ADCState = 1; //Set flag to indicate ADC is busy - ADC ISR (interrupt) will clear it
        ADC_startConversion(ADC_BASE, ADC_SINGLECHANNEL);
    }
}


int getAverageSensorReading(int sensor){
    int i;
    int total = 0;
    if (sensor == lightDex)
    {
        for (i = 0; i < NUM_SAMPLES_TO_AVERAGE; i++)
        {
            total += averagedValues->light[i];
        }
    }
    else if (sensor == tempZoneOneDex)
    {
        for (i = 0; i < NUM_SAMPLES_TO_AVERAGE; i++)
        {
            total += averagedValues->tempZoneOne[i];
        }
    }
    else if (sensor == tempZoneTwoDex)
    {
        for (i = 0; i < NUM_SAMPLES_TO_AVERAGE; i++)
        {
            total += averagedValues->tempZoneTwo[i];
        }
    }
    else if (sensor == moistureZoneOneDex)
    {
        for (i = 0; i < NUM_SAMPLES_TO_AVERAGE; i++)
        {
            total += averagedValues->moistureZoneOne[i];
        }
    }
    else if (sensor == moistureZoneTwoDex)
    {
        for (i = 0; i < NUM_SAMPLES_TO_AVERAGE; i++)
        {
            total += averagedValues->moistureZoneTwo[i];
        }
    }
    return (total / NUM_SAMPLES_TO_AVERAGE);
}

int storeSensorReadings(int sensor, int i)
{
    read_adc();
    if (sensor == lightDex)
    {
        averagedValues->light[i] = ADCResult;
    }
    else if (sensor == tempZoneOneDex)
    {
        // assuming 10mv/c
        averagedValues->tempZoneOne[i] = (ADCResult/10);
    }
    else if (sensor == tempZoneTwoDex)
    {
        // assuming 10mv/c
        averagedValues->tempZoneTwo[i] = ADCResult/10;
    }
    else if (sensor == moistureZoneOneDex)
    {
        // near 0 when not in water
        // ~500-700 when in water
        // displays as a percentage out of 700
        averagedValues->moistureZoneOne[i] = ADCResult/7;
    }
    else if (sensor == moistureZoneTwoDex)
    {
        // near 0 when not in water
        // ~500-700 when in water
        // displays as a percentage out of 700
        averagedValues->moistureZoneTwo[i] = ADCResult/7;
    }
    return 1;
}

int isDaytime(){
    lightVal = getAverageSensorReading(lightDex);
    isOver = lightVal > 700;
    return isOver;
}

void zone_select(void)
{
    if ((! GPIO_getInputPinValue(SW1_PORT, SW1_PIN)) && (! buttonPress)) /* button press */
    {
        buttonPress = true;

        if (! zone) /* zone 1 -> zone 2*/
        {
            zone = zone2;
            displayScrollText("ZONE 2");
        }
        else /* zone 2 -> zone 1*/
        {
            zone = zone1;
            displayScrollText("ZONE 1");
        }
    }
    else if ((GPIO_getInputPinValue(SW1_PORT, SW1_PIN)) && (buttonPress)) /* botton release */
        buttonPress = false;
}


/* LCD display */
void displayLCD(_Bool zone, unsigned char *temp, unsigned char *soil)
{
    showChar('T', pos1);
    showChar('M', pos4);

    if (! zone) /* zone 1*/
    {
        showChar(((temp[0]/10)+'0'), pos2);
        showChar(((temp[0]%10)+'0'), pos3);
        showChar(((soil[0]/10)+'0'), pos5);
        showChar(((soil[0]%10)+'0'), pos6);
    }
    else /* zone 2*/
    {
        showChar(((temp[1]/10)+'0'), pos2);
        showChar(((temp[1]%10)+'0'), pos3);
        showChar(((soil[1]/10)+'0'), pos5);
        showChar(((soil[1]%10)+'0'), pos6);
    }
}

void set_led_ind(int selected, int state)
{
    switch (selected)
    {
    case irri1:
        if (state == 0)
        {
            GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN2);
        }
        else
        {
            GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN2);
        }
        break;
    case irri2:
        if (state == 0)
        {
            GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN3);
        }
        else
        {
            GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN3);
        }
        break;
    case vent1:
        if (state == 0)
        {
            GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN3);
        }
        else
        {
            GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN3);
        }
        break;
    case vent2:
        if (state == 0)
        {
            GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN4);
        }
        else
        {
            GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN4);
        }
        break;
    }
    return;
}

