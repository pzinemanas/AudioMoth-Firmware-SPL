/* ----------------------------------------------------------------------
* Copyright (C) 2020 Pablo Zinemanas. All rights reserved.
*
* $Date:        26. February 2020
* $Revision:    V1.0.0
*
* Project:      AudioMoth-Firmware-SPL
* Title:        spl.h
*
* Description:  This library includes functions to do the microphone
*               compensation filter, A-weigthing filter and save SPL
*               to SD memory.
*
* pablo.zinemanas@upf.edu
 * -------------------------------------------------------------------- */

#ifndef INC_SPL_H_
#define INC_SPL_H_

#include <time.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* dBa filter constant */
#define GA                                  1.2589254117941673f
#define PI                                  3.141592653589793238462f
#define CALdBA_low                          82.5f
#define CALdBA_low_med                      78.4f
#define CALdBA_med                          71.8f
#define CALdBA_med_high                     67.8f
#define CALdBA_high                         65.6f

#define LOG_BUFFER_LENGTH                   50

/* Mic compensation filter */
void SPL_reset_compensation_filter();
void SPL_init_compensation_filter(float fs);
float SPL_compensation_mic_filter_step(float sample);
void SPL_find_calibration_offset(int gain);

/* dBA filter */
void SPL_reset_dBA_filter();
void SPL_init_dba_filter(float fs);
float SPL_dBA_filter_step(float sample);

/* LOG to SD*/
void SPL_write_log(uint32_t currentTime, float value);

#endif /* INC_SPL_H_ */
