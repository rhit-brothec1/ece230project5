/*
 * Author:      Cooper Brotherton
 * Date:        February 1, 2021
 * Libraries:   GPIO and Timer A from DriverLib
 */
/******************************************************************************
 * MSP432 Project 5 ECE230 Winter 2020-2021
 *
 * Description: Potentiometer circuit connected to PX.Y, photoresistor circuit
 *              connected to PX.Y. S1 toggles which analog output is shown on
 *              the LCD.
 *
 *                MSP432P401
 *             ------------------
 *         /|\|            P4.4  |---> D7
 *          | |            P4.5  |---> D6
 *          --|RST         P4.6  |---> D5
 *            |            P4.7  |---> D4
 *       S1-->|P1.1        P3.3  |---> RS
 *            |            P3.2  |---> E
 *      Pot-->|P6.1              |
 *    Photo-->|P6.0              |
 *******************************************************************************/
/* DriverLib Includes */
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

/* Standard Includes */
#include <stdint.h>
#include <stdbool.h>

#include "Switch.h"
#include "lcd.h"
#include "delays.h"

static volatile uint16_t digitalValue;
static volatile uint32_t analogValue;
bool usePotentiometerCircuit;
bool debounced;

/*!
 * \brief This function intializes the peripherials for the project
 *
 * This function initializes S1 with interrupts, turns on ADC for P6.0 and P6.1,
 * starts Timer32 for ADC refresh rate, and TimerA0 for debouncing S1.
 *
 * \return None
 */
void setup(void)
{
    usePotentiometerCircuit = true;
    debounced = true;

    Switch_init();

    // Stop Watchdog
    WDT_A_holdTimer();

    // ADC initialization
    // Enabling the FPU for floating point operation
    FPU_enableModule();
    FPU_enableLazyStacking();

    // Initializing ADC
    ADC14_enableModule();
    ADC14_initModule(ADC_CLOCKSOURCE_MCLK, ADC_PREDIVIDER_1, ADC_DIVIDER_1, 0);

    // Configuring GPIOs (6.0, 6.1; A15, A14)
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P6,
                                               GPIO_PIN0 | GPIO_PIN1,
                                               GPIO_TERTIARY_MODULE_FUNCTION);

    // Configuring ADC
    ADC14_configureMultiSequenceMode(ADC_MEM14, ADC_MEM15, false);
    ADC14_configureConversionMemory(ADC_MEM14,
                                    ADC_VREFPOS_AVCC_VREFNEG_VSS,
                                    ADC_INPUT_A14, false);
    ADC14_configureConversionMemory(ADC_MEM15,
                                    ADC_VREFPOS_AVCC_VREFNEG_VSS,
                                    ADC_INPUT_A15, false);
    ADC14_enableSampleTimer(ADC_MANUAL_ITERATION);
    ADC14_enableConversion();
    ADC14_toggleConversionTrigger();
    ADC14_enableInterrupt(ADC_INT14);
    ADC14_enableInterrupt(ADC_INT15);
    Interrupt_enableInterrupt(INT_ADC14);

    // 1s Timer32 in One-shot mode
    Timer32_initModule(TIMER32_0_BASE, TIMER32_PRESCALER_1, TIMER32_32BIT,
    TIMER32_PERIODIC_MODE);
    Timer32_setCount(TIMER32_0_BASE, CS_getMCLK());
    Timer32_startTimer(TIMER32_0_BASE, true);

    // 5ms TimerA for debounce
    const Timer_A_UpModeConfig upConfig = { TIMER_A_CLOCKSOURCE_SMCLK,
                                            TIMER_A_CLOCKSOURCE_DIVIDER_1,
                                            15000,
                                            TIMER_A_TAIE_INTERRUPT_DISABLE,
                                            TIMER_A_CCIE_CCR0_INTERRUPT_ENABLE,
                                            TIMER_A_DO_CLEAR };
    Timer_A_configureUpMode(TIMER_A2_BASE, &upConfig);
    Timer_A_enableInterrupt(INT_TA2_0);

    // LCD initialization
    configLCD(GPIO_PORT_P3, GPIO_PIN3, GPIO_PORT_P3, GPIO_PIN2, GPIO_PORT_P4);
    initDelayTimer(CS_getMCLK());
    initLCD();

    Interrupt_enableMaster();
}

/*!
 * \brief This function updates the LCD based on the analog inputs
 *
 * This function waits until Timer32 has counted down (1 second), then updates
 * the LCD with the digital value from the analog circuit and the corresponding
 * converting analog value on the next line.
 *
 * \return None
 */
void loop(void)
{
    while (TIMER32_1->VALUE != 0)
    {
    }
    ADC14_toggleConversionTrigger();
    commandInstruction(CLEAR_DISPLAY_MASK, false);
    commandInstruction(RETURN_HOME_MASK, false);

    if (usePotentiometerCircuit)
    {
        printString("Pot: ", 5);
    }
    else
    {
        printString("Photo: ", 7);
    }
    // Display digital value
    char dnum[5];
    sprintf(dnum, "%d", digitalValue);
    printString(dnum, 7);
    commandInstruction(SET_CURSOR_MASK | LINE2_OFFSET, false);

    // Convert and print analog value
    printString("Analog: ", 8);
    analogValue = ((digitalValue * 3.3) / 16384) * 1000;
    char num[10] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    printChar(num[analogValue / 1000]);
    printChar('.');
    char anum[3];
    sprintf(anum, "%d", analogValue % 1000);
    printString(anum, 3);
    printString(" V", 2);
    // Restart timer (1 second)
    Timer32_setCount(TIMER32_0_BASE, CS_getMCLK());
}

int main(void)
{
    setup();

    while (1)
    {
        loop();
    }
}

/* !
 * \brief This function handles ADC conversions
 *
 * This function updates the digital value of the appropriate circuit. ADC_MEM14
 * is connected to a photoresistor and ADC_MEM15 is connected to a potentiometer.
 *
 * If the incorrect conversion is ready, it will trigger again to update to the
 * correct value faster.
 *
 * \return None
 */
void ADC14_IRQHandler(void)
{
    uint64_t status = MAP_ADC14_getEnabledInterruptStatus();
    MAP_ADC14_clearInterruptFlag(status);
    // Photoresistor
    if (ADC_INT14 & status)
    {
        if (!usePotentiometerCircuit)
        {
            digitalValue = MAP_ADC14_getResult(ADC_MEM14);
        }
        else
        {
            ADC14_toggleConversionTrigger();
        }
    }
    // Potentiometer
    if (ADC_INT15 & status)
    {
        if (usePotentiometerCircuit)
        {
            digitalValue = MAP_ADC14_getResult(ADC_MEM15);
        }
        else
        {
            ADC14_toggleConversionTrigger();
        }

    }
}

/*!
 * \brief This function toggles which analog circuit is used for input
 *
 * This function toggles whether the analog input is the potentiometer or the
 * photoresistor when S1 is pressed.
 *
 * \return None
 */
void PORT1_IRQHandler(void)
{
    uint32_t status = GPIO_getEnabledInterruptStatus(GPIO_PORT_P1);
    GPIO_clearInterruptFlag(GPIO_PORT_P1, status);

    if (debounced)
    {
        if (status & SWITCH_PIN)
        {
            usePotentiometerCircuit = !usePotentiometerCircuit;
        }
//        debounced = false;
        Timer_A_startCounter(TIMER_A2_BASE, TIMER_A_UP_MODE);
    }
}

/*!
 * \brief This function acts as a debounce function for S1
 *
 * This function turns debounced to true so S1 can take another press
 *
 * \return None
 */
void TA2_0_IRQHandler(void)
{
    debounced = true;
    Timer_A_stopTimer(TIMER_A0_BASE);
    Timer_A_clearCaptureCompareInterrupt(TIMER_A2_BASE,
    TIMER_A_CAPTURECOMPARE_REGISTER_0);
}
