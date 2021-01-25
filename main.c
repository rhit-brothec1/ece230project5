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

// A14->55->P6.1
// A15->54->P6.0

void setup(void)
{
    Switch_init();

    /* Stop Watchdog  */
    MAP_WDT_A_holdTimer();

    // LCD init
    configLCD(GPIO_PORT_P3, GPIO_PIN3, GPIO_PORT_P3, GPIO_PIN2, GPIO_PORT_P4);
    initDelayTimer(CS_getMCLK());
    initLCD();

    // ADC init
    curADCResult = 0;
    usePotentiometerCircuit = true;
    /* Setting Flash wait state */
    MAP_FlashCtl_setWaitState(FLASH_BANK0, 1);
    MAP_FlashCtl_setWaitState(FLASH_BANK1, 1);

    /* Setting DCO to 48MHz  */
    MAP_PCM_setPowerState(PCM_AM_LDO_VCORE1);
    MAP_CS_setDCOCenteredFrequency(CS_DCO_FREQUENCY_48);

    /* Enabling the FPU for floating point operation */
    MAP_FPU_enableModule();
    MAP_FPU_enableLazyStacking();

    //![Single Sample Mode Configure]
    /* Initializing ADC (MCLK/1/4) */
    MAP_ADC14_enableModule();
    MAP_ADC14_initModule(ADC_CLOCKSOURCE_MCLK, ADC_PREDIVIDER_1, ADC_DIVIDER_4,
                         0);

    /* Configuring GPIOs (6.0, 6.1; A15, A14) */
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(
            GPIO_PORT_P6, GPIO_PIN0 | GPIO_PIN1,
            GPIO_TERTIARY_MODULE_FUNCTION);

    /* Configuring ADC Memory */
    MAP_ADC14_configureMultiSequenceMode(ADC_MEM14, ADC_MEM15, true);
    MAP_ADC14_configureConversionMemory(ADC_MEM14, ADC_VREFPOS_AVCC_VREFNEG_VSS,
    ADC_INPUT_A14,
                                        false);
    MAP_ADC14_configureConversionMemory(ADC_MEM15, ADC_VREFPOS_AVCC_VREFNEG_VSS,
    ADC_INPUT_A15,
                                        false);

    /* Configuring Sample Timer */
    MAP_ADC14_enableSampleTimer(ADC_MANUAL_ITERATION);

    /* Enabling/Toggling Conversion */
    MAP_ADC14_enableConversion();
    MAP_ADC14_toggleConversionTrigger();

    /* Enabling interrupts */
    MAP_ADC14_enableInterrupt(ADC_INT14);
    MAP_ADC14_enableInterrupt(ADC_INT15);
    MAP_Interrupt_enableInterrupt(INT_ADC14);
    MAP_Interrupt_enableMaster();

    // 1s period?
    SysTick_enableModule();
    SysTick_setPeriod(3000000);
}

void loop(void)
{
    // TODO systick, update LCD using values?
    while (((SysTick->CTRL) & SysTick_CTRL_COUNTFLAG_Msk) == 0)
    {
        // TODO start at beginning
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
        printChar(':');
        printChar(' ');
        // https://stackoverflow.com/questions/46006861/converting-int-to-char-in-c
        // TODO int to char
        // TODO new line
        normalizedADCRes = (curADCResult * 3.3) / 16384;
        printChar('A');
        printChar('n');
        printChar('a');
        printChar('l');
        printChar('o');
        printChar('g');
        printChar(':');
        // TODO int to char
        printChar(' ');
        printChar('V');
    }
}

int main(void)
{
    setup();

    while (1)
    {
//        loop();

//        MAP_PCM_gotoLPM0();
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

        MAP_ADC14_toggleConversionTrigger();
    }
    // Potentiometer
    if (ADC_INT15 & status)
    {
        if (usePotentiometerCircuit)
        {
            curADCResult = MAP_ADC14_getResult(ADC_MEM15);
        }

        MAP_ADC14_toggleConversionTrigger();
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

    if (status & SWITCH_PIN)
    {
        usePotentiometerCircuit = !usePotentiometerCircuit;
    }
}
