/* ----------------------------------------------------------------------
 * Copyright (C) 2020 Pablo Zinemanas. All rights reserved.
 *
 * $Date:        26. February 2020
 * $Revision:    V1.0.0
 *
 * Project:      AudioMoth-Firmware-SPL
 * Title:        spl.c
 *
 * pablo.zinemanas@upf.edu
 * -------------------------------------------------------------------- */

/* dBA filter */
#include "spl.h"
#include "audioMoth.h"

/* Temp variables of dbA filter */
static float fRec0[3], fRec1[2], fRec2[2], fRec3[3];

/* dbA filter coefficients */
static float a1[2], b1[3], a2, b2[2], a3, b3[2], a4[2], b4[3];

/* dbA filter cut frequencies */
static float w1, w2, w3, w4;

/* global variables of SPL and number of samples */
extern float spl;
extern uint32_t n;

/* file name and buffer for SD memory */
static char logFilename[20];
static char logBuffer[LOG_BUFFER_LENGTH];

/* Offset of the SPL measure (found in calibration) */
extern float cal_offset;

/* Temp variables of microphone compensation filter */
float fRec0_comp[2];
float fRec1_comp[2];

/* Coefficients of the compensation filter */
float a_comp;
float b_comp;
float G_comp;

void float_to_string(char* string, float value);

/* Reset Compensation filter */
void SPL_reset_compensation_filter() {
	for (int ix = 0; (ix < 2); ix = (ix + 1)) {
		fRec0_comp[ix] = 0.0f;
		fRec1_comp[ix] = 0.0f;
	}
}

/* Init Compensation filter */
void SPL_init_compensation_filter(float fs) {
	SPL_reset_compensation_filter();

	if (fs == 8000) {
		a_comp = -0.97;
		b_comp = -0.948;
		G_comp = 1.0198586881755944;
	} else if (fs == 16000) {
		a_comp = -0.98;
		b_comp = -0.97;
		G_comp = 1.0068852990025103;
	} else if (fs == 32000) {
		a_comp = -0.998;
		b_comp = -0.9895;
		G_comp = 1.005779339050867;
	} else if (fs == 48000) {
		a_comp = -0.999;
		b_comp = -0.993;
		G_comp = 1.0033250259620856;
	} else if (fs == 96000) {
		a_comp = -0.999;
		b_comp = -0.9964;
		G_comp = 1.0;
	} else if (fs == 192000) {
		a_comp = -0.9995;
		b_comp = -0.9979;
		G_comp = 0.9992823605244088;
	} else if (fs == 256000) {
		a_comp = -0.9995;
		b_comp = -0.9985;
		G_comp = 0.9992823605244088;
	} else if (fs == 192000) {
		a_comp = -0.9995;
		b_comp = -0.9979;
		G_comp = 0.998417420445598;
	} else if (fs == 384000) {
		a_comp = -0.9996;
		b_comp = -0.99895;
		G_comp = 0.9981619947924345;
	}

}

float SPL_compensation_mic_filter_step(float sample) {
	float filtered_sample;
	fRec1_comp[0] = (sample - (a_comp * fRec1_comp[1]));
	fRec0_comp[0] = ((fRec1_comp[0] + (b_comp * fRec1_comp[1]))
			- (a_comp * fRec0_comp[1]));
	filtered_sample = (float) (G_comp
			* (fRec0_comp[0] + (b_comp * fRec0_comp[1])));
	fRec1_comp[1] = fRec1_comp[0];
	fRec0_comp[1] = fRec0_comp[0];
	return filtered_sample;
}

/* Reset A-weighing filter */
void SPL_reset_A_weighting_filter() {
	spl = 0.0f;
	n = 0;

	for (int l0 = 0; (l0 < 3); l0 = (l0 + 1)) {
		fRec0[l0] = 0.0f;
		fRec3[l0] = 0.0f;
	}
	for (int l1 = 0; (l1 < 2); l1 = (l1 + 1)) {
		fRec1[l1] = 0.0f;
		fRec2[l1] = 0.0f;
	}
}

/* Init A-weighing filter */
void SPL_init_A_weighting_filter(float fs) {

	SPL_reset_A_weighting_filter();

	float f1 = 20.6f;
	float f2 = 107.7f;
	float f3 = 737.9f;
	float f4 = 12194.0f;

	w1 = 2.0f * PI * f1;
	w2 = 2.0f * PI * f2;
	w3 = 2.0f * PI * f3;
	w4 = 2.0f * PI * f4;

	float a0;
	a0 = pow((w1 + 2.0f * fs), 2);
	a1[0] = 2.0f * (pow(w1, 2) - 4.0f * pow(fs, 2)) / a0;
	a1[1] = pow(w1 - 2.0f * fs, 2) / a0;
	b1[0] = 2.0f * fs / a0;
	b1[1] = 0.0f;
	b1[2] = -2.0f * fs / a0;

	a0 = w2 + 2.0f * fs;
	b2[0] = 2.0f * fs / a0;
	b2[1] = -2.0f * fs / a0;
	a2 = (w2 - 2.0f * fs) / a0;

	a0 = w3 + 2.0f * fs;
	b3[0] = 2.0f * fs / a0;
	b3[1] = -2.0f * fs / a0;
	a3 = (w3 - 2.0f * fs) / a0;

	a0 = pow((w4 + 2.0f * fs), 2);
	a4[0] = 2.0f * (pow(w4, 2) - 4.0f * pow(fs, 2)) / a0;
	a4[1] = pow(w4 - 2.0f * fs, 2) / a0;
	b4[0] = 2.0f * fs / a0;
	b4[1] = 0.0f;
	b4[2] = -2.0f * fs / a0;

	sprintf(logFilename, "SPL.log");
}

float SPL_A_weighting_filter_step(float sample) {
	float filteredOutput_A;
	fRec3[0] = (sample - ((a1[0] * fRec3[1]) + (a1[1] * fRec3[2])));
	fRec2[0] = ((((b1[0] * fRec3[0]) + (b1[1] * fRec3[1])) + (b1[2] * fRec3[2]))
			- (a2 * fRec2[1]));
	fRec1[0] = (((b2[0] * fRec2[0]) + (b2[1] * fRec2[1])) - (a3 * fRec1[1]));
	fRec0[0] = (((b3[0] * fRec1[0]) + (b3[1] * fRec1[1]))
			- ((a4[0] * fRec0[1]) + (a4[1] * fRec0[2])));
	filteredOutput_A =
			(float) ((GA * w4 * w4
					* (((b4[0] * fRec0[0]) + (b4[1] * fRec0[1]))
							+ (b4[2] * fRec0[2]))));

	fRec3[2] = fRec3[1];
	fRec3[1] = fRec3[0];
	fRec2[1] = fRec2[0];
	fRec1[1] = fRec1[0];
	fRec0[2] = fRec0[1];
	fRec0[1] = fRec0[0];

	return filteredOutput_A;
}

/* Find calibration offset in function of gain */
void SPL_find_calibration_offset(int gain) {
	cal_offset = 0.0;
	switch (gain) {
	case 0:
		cal_offset = CALdBA_low;
		break;
	case 1:
		cal_offset = CALdBA_low_med;
		break;
	case 2:
		cal_offset = CALdBA_med;
		break;
	case 3:
		cal_offset = CALdBA_med_high;
		break;
	case 4:
		cal_offset = CALdBA_high;
		break;
	}
}

/* Convert float number to string */
void float_to_string(char* string, float value) {
	char *tmpSign = (value < 0) ? "-" : "";
	float tmpVal = (value < 0) ? -value : value;

	int tmpInt1 = tmpVal;                  // Get the integer (678).
	float tmpFrac = tmpVal - tmpInt1;      // Get fraction (0.0123).
	int tmpInt2 = trunc(tmpFrac * 10000);  // Turn into integer (123).

	sprintf(string, "%s%d.%01d ", tmpSign, tmpInt1, tmpInt2);
}

/* Append message (spl value) to logfile */
void SPL_write_log(uint32_t currentTime) {

	AudioMoth_enableFileSystem();

	AudioMoth_appendFile(logFilename);

	struct tm *time = gmtime((time_t*) &currentTime);

	sprintf(logBuffer, "%02d/%02d/%04d %02d:%02d:%02d: ", time->tm_mday,
			time->tm_mon + 1, time->tm_year + 1900, time->tm_hour, time->tm_min,
			time->tm_sec);

	AudioMoth_writeToFile(logBuffer, strnlen(logBuffer, LOG_BUFFER_LENGTH));

	float_to_string(logBuffer, spl);
	AudioMoth_writeToFile(logBuffer, strnlen(logBuffer, LOG_BUFFER_LENGTH));

	AudioMoth_writeToFile("\n", 1);

	AudioMoth_closeFile();
}

/* Update SPL value */
void SPL_update_value(float value) {
	// Mean value of SPL sequence
	spl = (n*spl + value*value)/(n+1); // y[n] = (n*y[n]+x^2[n])/(n+1)
	n += 1;
}

/* convert SPL value to dB */
void SPL_to_dB() {
	spl = 10.0f*log10f(spl) + cal_offset; //to dB
}
