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
*               compensation filter, A-weighting filter and save SPL
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

/* dBa filter constants */
#define GA                                  1.2589254117941673f
#define PI                                  3.141592653589793238462f
#define CALdBA_low                          82.5f
#define CALdBA_low_med                      78.4f
#define CALdBA_med                          71.8f
#define CALdBA_med_high                     67.8f
#define CALdBA_high                         65.6f
#define LOG_BUFFER_LENGTH                   50

/* Mic compensation filter */

/**
 * Reset compensation filter.
 *
 * Set temporal variables of the compensation filter to zero to be ready for
 * the next signal. Has to be called when the program starts and when the filtering
 * is finished.
 *
 */
void SPL_reset_compensation_filter();

/**
 * Init compensation filter.
 *
 * Initialize the coefficients of the compensation filter in function of the
 * sampling rate.
 *
 * @param fs Sampling rate in Hz.
 */
void SPL_init_compensation_filter(float fs);

/**
 * Step of the compensation filter.
 *
 * Calculates the filter output given the input in a specific time, n.
 * After calculation, update the temp filter variables.
 *
 * @param sample Sample of the input signal, x[n].
 * @return The output of the filter in time n, y[n].
 */
float SPL_compensation_mic_filter_step(float sample);

/**
 * Find the calibration offset.
 *
 * The last step of the SPL estimation, is to sum an offset found in the
 * calibration process. This function set this offset in function of the
 * gain configured in the AudioMoth.
 *
 * @param gain Gain configured in AudioMoth (0,1,2,3,4).
 */
void SPL_find_calibration_offset(int gain);

/* dBA filter */

/**
 * Reset dbA filter.
 *
 * Set temporal variables of the dbA filter to zero to be ready for
 * the next signal. Has to be called when the program starts and when the filtering
 * is finished.
 *
 */
void SPL_reset_dBA_filter();

/**
 * Init dBa filter.
 *
 * Initialize the coefficients of the dBa filter in function of the
 * sampling rate.
 *
 * @param fs Sampling rate in Hz.
 */
void SPL_init_dba_filter(float fs);

/**
 * Step of the dBa filter.
 *
 * Calculates the filter output given the input in a specific time, n.
 * After calculation, update the temp filter variables.
 *
 * @param sample Sample of the input signal, x[n].
 * @return The output of the filter in time n, y[n].
 */
float SPL_dBA_filter_step(float sample);

/**
 * Append a line in the LogFile.
 *
 * Append a line in the LogFile with a timestamp and the SPL value in dB.
 *
 * @param currentTime Time when the record process started.
 * @param value SPL value
 */
void SPL_write_log(uint32_t currentTime, float value);


#endif /* INC_SPL_H_ */
