/*
 * Switches.c
 *
 *  Created on: Dec 18, 2020
 *  Edited on:  Jan 16, 2021
 *      Author: Cooper Brotherton
 */
/* DriverLib Includes */
#include <Switch.h>
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

void Switch_init(void)
{
    GPIO_setAsInputPinWithPullUpResistor(SWITCH_PORT, SWITCH_PIN);
    GPIO_clearInterruptFlag(SWITCH_PORT, SWITCH_PIN);
    GPIO_enableInterrupt(SWITCH_PORT, SWITCH_PIN);
    Interrupt_enableInterrupt(INT_PORT1);
}
