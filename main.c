/*
 * Author:      Cooper Brotherton
 * Date:        January 23, 2021
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
#include <Switch.h>

#include "lcd.h"
#include "delays.h"

static volatile uint16_t curADCResult;
static volatile float normalizedADCRes;
bool usePotentiometerCircuit;
bool update;
bool debounced;

void setup(void)
{
    curADCResult = 0;
    usePotentiometerCircuit = true;
    update = true;
    debounced = true;

    Switch_init();

    /* Stop Watchdog  */
    WDT_A_holdTimer();

    // ADC init

    /* Setting Flash wait state */
    FlashCtl_setWaitState(FLASH_BANK0, 1);
    FlashCtl_setWaitState(FLASH_BANK1, 1);

    /* Setting DCO to 48MHz  */
//    PCM_setPowerState(PCM_AM_LDO_VCORE1);
//    CS_setDCOCenteredFrequency(CS_DCO_FREQUENCY_48);
    /* Enabling the FPU for floating point operation */
    FPU_enableModule();
    FPU_enableLazyStacking();

    /* Initializing ADC (MCLK/1/4) */
    ADC14_enableModule();
    ADC14_initModule(ADC_CLOCKSOURCE_MCLK, ADC_PREDIVIDER_1, ADC_DIVIDER_4, 0);

    /* Configuring GPIOs (6.0, 6.1; A15, A14) */
    GPIO_setAsPeripheralModuleFunctionInputPin(
    GPIO_PORT_P6,
                                               GPIO_PIN0 | GPIO_PIN1,
                                               GPIO_TERTIARY_MODULE_FUNCTION);

    /* Configuring ADC Memory */
    ADC14_configureMultiSequenceMode(ADC_MEM14, ADC_MEM15, true);
    ADC14_configureConversionMemory(ADC_MEM14, ADC_VREFPOS_AVCC_VREFNEG_VSS,
    ADC_INPUT_A14,
                                    false);
    ADC14_configureConversionMemory(ADC_MEM15, ADC_VREFPOS_AVCC_VREFNEG_VSS,
    ADC_INPUT_A15,
                                    false);

    /* Configuring Sample Timer */
    ADC14_enableSampleTimer(ADC_MANUAL_ITERATION);

    /* Enabling/Toggling Conversion */
    ADC14_enableConversion();
    ADC14_toggleConversionTrigger();

    /* Enabling interrupts */
    ADC14_enableInterrupt(ADC_INT14);
    ADC14_enableInterrupt(ADC_INT15);
    Interrupt_enableInterrupt(INT_ADC14);

    // 1s periodic Timer32
    Timer32_initModule(TIMER32_0_BASE, TIMER32_PRESCALER_1, TIMER32_32BIT,
    TIMER32_PERIODIC_MODE);
    Timer32_setCount(TIMER32_0_BASE, CS_getMCLK());
    Timer32_enableInterrupt(TIMER32_0_BASE);
    TIMER32_1->INTCLR = 1;
    Timer32_startTimer(TIMER32_0_BASE, false);

    // 5ms TimerA
    const Timer_A_UpModeConfig upConfig = { TIMER_A_CLOCKSOURCE_SMCLK,
    TIMER_A_CLOCKSOURCE_DIVIDER_1,
                                            15000,
                                            TIMER_A_TAIE_INTERRUPT_DISABLE,
                                            TIMER_A_CCIE_CCR0_INTERRUPT_ENABLE,
                                            TIMER_A_DO_CLEAR };
    Timer_A_configureUpMode(TIMER_A2_BASE, &upConfig);
    Timer_A_enableInterrupt(INT_TA2_0);

    // LCD init
    configLCD(GPIO_PORT_P3, GPIO_PIN3, GPIO_PORT_P3, GPIO_PIN2, GPIO_PORT_P4);
    initDelayTimer(CS_getMCLK());
    initLCD();

    Interrupt_enableMaster();
}

void loop(void)
{
    while (TIMER32_1->VALUE > 100)
    {
    }
    ADC14_toggleConversionTrigger();
//    update = false;
    commandInstruction(CLEAR_DISPLAY_MASK);
    delayMicroSec(SHORT_INSTR_DELAY);
    commandInstruction(RETURN_HOME_MASK);
    delayMicroSec(SHORT_INSTR_DELAY);

    if (usePotentiometerCircuit)
    {
        printChar('P');
        printChar('o');
        printChar('t');
    }
    else
    {
        printChar('P');
        printChar('h');
        printChar('o');
        printChar('t');
        printChar('o');
    }
    char dnum[7];
    sprintf(dnum, ": %d", curADCResult);
    printString(dnum, 7);
    commandInstruction(SET_CURSOR_MASK | LINE2_OFFSET);
    delayMicroSec(SHORT_INSTR_DELAY);

    normalizedADCRes = (curADCResult * 3.3) / 16384;
    char anum[13];
    sprintf(anum, "Analog: %f", normalizedADCRes);
    printString(anum, 13);
    printChar(' ');
    printChar('V');
}

int main(void)
{
    setup();

    while (1)
    {
        loop();
    }
}

/* ADC Interrupt Handler. This handler is called whenever there is a conversion
 * that is finished for ADC_MEM14 or ADC_MEM15.
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
            curADCResult = MAP_ADC14_getResult(ADC_MEM14);
        }
    }
    // Potentiometer
    if (ADC_INT15 & status)
    {
        if (usePotentiometerCircuit)
        {
            curADCResult = MAP_ADC14_getResult(ADC_MEM15);
        }

    }
}

/*!
 * \brief This function toggles which analog circuit is used for input
 *
 * This function toggles whether the circuit used is the potentiometer or the
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

void T32_INT1_IRQHandler(void)
{
    // write to clear register
    TIMER32_1->INTCLR = 1;
    update = true;

}

void TA2_0_IRQHandler(void)
{
    debounced = true;
//    Timer_A_stopTimer(TIMER_A0_BASE);
    Timer_A_clearCaptureCompareInterrupt(TIMER_A2_BASE,
    TIMER_A_CAPTURECOMPARE_REGISTER_0);
}
