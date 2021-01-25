/*
 * Switches.h
 *
 *  Created on: Dec 18, 2020
 *  Edited on:  Jan 23, 2021
 *      Author: Cooper Brotherton
 */

#ifndef SWITCH_H_
#define SWITCH_H_

#ifdef __cplusplus
extern "C" {
#endif

/* DriverLib Includes */
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

#define SWITCH_PORT                                                 GPIO_PORT_P1
#define SWITCH_PIN                                                  GPIO_PIN1

/*!
 * \brief This function configures the switches as inputs
 *
 * This function configures P1.1 as an input pin with a pull-up resistor.
 * Port 1 interrupts are enabled.
 *
 * \return None
 */
extern void Switch_init(void);

#ifdef __cplusplus
}
#endif

#endif /* SWITCH_H_ */
