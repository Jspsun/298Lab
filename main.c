#include "main.h"
#include "driverlib/driverlib.h"
#include <stdlib.h>
#include "hal_LCD.h"
#include <stdio.h>

#define NUM_SAMPLES_TO_AVERAGE 10 //TODO: tweak if needed
#define lightDex 0
#define tempZoneOneDex 1
#define tempZoneTwoDex 2
#define moistureZoneOneDex 3
#define moistureZoneTwoDex 4

char ADCState = 0; //Busy state of the ADC
int16_t ADCResult = 0; //Storage for the ADC conversion result
int WAIT_TIMER = 50000; //TODO: fine tune this number
int NUM_SENSORS = 5;
int zoneToDisplay = 0;
enum Servo servo;

typedef struct
{
    float light[NUM_SAMPLES_TO_AVERAGE];
    float tempZoneOne[NUM_SAMPLES_TO_AVERAGE];
    float tempZoneTwo[NUM_SAMPLES_TO_AVERAGE];
    float moistureZoneOne[NUM_SAMPLES_TO_AVERAGE];
    float moistureZoneTwo[NUM_SAMPLES_TO_AVERAGE];
} Values;

int delay(int n);
int setMux(int n);
int storeSensorReadings(Values* averagedValues, int sensor, int i);
float getAverageSensorReading(Values* averagedValues, int sensor);

float getAverageSensorReading(Values* averagedValues, int sensor){
    int i = 0;
    float total = 0.0;
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

int storeSensorReadings(Values* averagedValues, int sensor, int i)
{
    read_adc();
    if (sensor == lightDex)
    {
        averagedValues->light[i] = ADCResult;
    }
    else if (sensor == tempZoneOneDex)
    {
        averagedValues->tempZoneOne[i] = ADCResult;
    }
    else if (sensor == tempZoneTwoDex)
    {
        averagedValues->tempZoneTwo[i] = ADCResult;
    }
    else if (sensor == moistureZoneOneDex)
    {
        averagedValues->moistureZoneOne[i] = ADCResult;
    }
    else if (sensor == moistureZoneTwoDex)
    {
        averagedValues->moistureZoneTwo[i] = ADCResult;
    }
    return 1;
}

void main(void)
{
    Values* averagedValues = malloc(sizeof(Values));

    char buttonState = 0; //Current button press state (to allow edge detection)
    int muxState = 0;
    int displayDur = 0;
    servo = irri1;
    /*
     * Functions with two underscores in front are called compiler intrinsics.
     * They are documented in the compiler user guide, not the IDE or MCU guides.
     * They are a shortcut to insert some assembly code that is not really
     * expressible in plain C/C++. Google "MSP430 Optimizing C/C++ Compiler
     * v18.12.0.LTS" and search for the word "intrinsic" if you want to know
     * more.
     * */

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

    /*
     * The MSP430 MCUs have a variety of low power modes. They can be almost
     * completely off and turn back on only when an interrupt occurs. You can
     * look up the power modes in the Family User Guide under the Power Management
     * Module (PMM) section. You can see the available API calls in the DriverLib
     * user guide, or see "pmm.h" in the driverlib directory. Unless you
     * purposefully want to play with the power modes, just leave this command in.
     */
    PMM_unlockLPM5(); //Disable the GPIO power-on default high-impedance mode to activate previously configured port settings

    //All done initializations - turn interrupts back on.
    __enable_interrupt();

    // preloop to get some average values
    int i = 0;
    for (i = 0; i < NUM_SAMPLES_TO_AVERAGE; i++)
    {
        delay(WAIT_TIMER);
        int sensor = NUM_SENSORS;
        for (sensor = 0; sensor < NUM_SENSORS; sensor++)
        {
            setMux(sensor);
            delay(1000);
            storeSensorReadings(averagedValues, sensor, i);
        }
    }

    while (1)
    {
        // toggle the display for each zone
        //Buttons SW1 and SW2 are active low (1 until pressed, then 0)
        if ((GPIO_getInputPinValue(SW1_PORT, SW1_PIN) == 1)
                & (buttonState == 0)) //Look for rising edge
        {
            output_pwm_off();

            buttonState = 1;                //Capture new button state

        }
        if ((GPIO_getInputPinValue(SW1_PORT, SW1_PIN) == 0) // if button pressed, change state
        & (buttonState == 1)) //Look for falling edge
        {
            output_pwm_on();
            buttonState = 0;                          //Capture new button state
            // --------------------------------
            muxState = (muxState + 1) % 5;
            setMux(muxState); // set GPIO pins to value of mux toggle
            displayDur = 0; // force re-set of lcd display

        }



        // process readings

        //Start an ADC conversion (if it's not busy) in Single-Channel, Single Conversion Mode
        read_adc();


        if (displayDur == 0)
        {
            showHex((int) ADCResult); //Put the previous result on the LCD display
            displayDur = 1500;
        }
        displayDur -= 1;

        /*
         * You can use the following code if you plan on only using interrupts
         * to handle all your system events since you don't need any infinite loop of code.
         *
         * //Enter LPM0 - interrupts only
         * __bis_SR_register(LPM0_bits);
         * //For debugger to let it know that you meant for there to be no more code
         * __no_operation();
         */

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

int delay(int n)
{
    int i = 0;
    for (i = 0; i < n; i++)
    {
        i += 1;
        i -= 1;
    }
    return 1;
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



void read_adc(){
    if (ADCState == 0) {
        ADCState = 1; //Set flag to indicate ADC is busy - ADC ISR (interrupt) will clear it
        ADC_startConversion(ADC_BASE, ADC_SINGLECHANNEL);
    }
}
