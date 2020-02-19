/****************************************************************************
 * main.c
 * openacousticdevices.info
 * June 2017
 *
 * Edited by Pablo Zinemanas, 02/2020
 *****************************************************************************/

#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "audioMoth.h"

#include <math.h>

/* Sleep and LED constants */

#define DEFAULT_WAIT_INTERVAL               1

#define WAITING_LED_FLASH_INTERVAL          2
#define WAITING_LED_FLASH_DURATION          10

#define LOW_BATTERY_LED_FLASHES             10

#define SHORT_LED_FLASH_DURATION            100
#define LONG_LED_FLASH_DURATION             500

/* Useful time constants */

#define SECONDS_IN_MINUTE                   60
#define SECONDS_IN_HOUR                     (60 * SECONDS_IN_MINUTE)
#define SECONDS_IN_DAY                      (24 * SECONDS_IN_HOUR)

/* SRAM buffer constants */

#define NUMBER_OF_BUFFERS                   8
#define EXTERNAL_SRAM_SIZE_IN_SAMPLES       (AM_EXTERNAL_SRAM_SIZE_IN_BYTES / 2)
#define NUMBER_OF_SAMPLES_IN_BUFFER         (EXTERNAL_SRAM_SIZE_IN_SAMPLES / NUMBER_OF_BUFFERS)
#define NUMBER_OF_SAMPLES_IN_DMA_TRANSFER   1024
#define NUMBER_OF_BUFFERS_TO_SKIP           1

/* WAV header constant */

#define PCM_FORMAT                          1
#define RIFF_ID_LENGTH                      4
#define LENGTH_OF_ARTIST                    32
#define LENGTH_OF_COMMENT                   256

/* USB configuration constant */

#define MAX_START_STOP_PERIODS              5

/* DC filter constant */

#define DC_BLOCKING_FACTOR                  0.995f

/* dBa filter constant */
#define GA                                  1.2589254117941673f
#define PI                                  3.141592653589793238462f
#define CALdBA_low                          82.5f
#define CALdBA_low_med                      78.4f
#define CALdBA_med                          71.8f
#define CALdBA_med_high                     67.8f
#define CALdBA_high                         65.6f

#define LOG_BUFFER_LENGTH                   50

/* Useful macros */

#define FLASH_LED(led, duration) { \
    AudioMoth_set ## led ## LED(true); \
    AudioMoth_delay(duration); \
    AudioMoth_set ## led ## LED(false); \
}

#define RETURN_ON_ERROR(fn) { \
    bool success = (fn); \
    if (success != true) { \
        if (configSettings->enableBatteryCheck ) { \
            AudioMoth_disableBatteryMonitor(); \
        } \
        FLASH_LED(Both, LONG_LED_FLASH_DURATION) \
        return SDCARD_WRITE_ERROR; \
    } \
}

#define SAVE_SWITCH_POSITION_AND_POWER_DOWN(duration) { \
    *previousSwitchPosition = switchPosition; \
    AudioMoth_powerDownAndWake(duration, true); \
}

#define MAX(a,b) (((a) > (b)) ? (a) : (b))

#define MIN(a,b) (((a) < (b)) ? (a) : (b))



/* Recording state enumeration */

typedef enum {
	RECORDING_OKAY, SWITCH_CHANGED, SDCARD_WRITE_ERROR, BATTERY_CHECK
} AM_recordingState_t;

/* WAV header */

#pragma pack(push, 1)

typedef struct {
	char id[RIFF_ID_LENGTH];
	uint32_t size;
} chunk_t;

typedef struct {
	chunk_t icmt;
	char comment[LENGTH_OF_COMMENT];
} icmt_t;

typedef struct {
	chunk_t iart;
	char artist[LENGTH_OF_ARTIST];
} iart_t;

typedef struct {
	uint16_t format;
	uint16_t numberOfChannels;
	uint32_t samplesPerSecond;
	uint32_t bytesPerSecond;
	uint16_t bytesPerCapture;
	uint16_t bitsPerSample;
} wavFormat_t;

typedef struct {
	chunk_t riff;
	char format[RIFF_ID_LENGTH];
	chunk_t fmt;
	wavFormat_t wavFormat;
	chunk_t list;
	char info[RIFF_ID_LENGTH];
	icmt_t icmt;
	iart_t iart;
	chunk_t data;
} wavHeader_t;

#pragma pack(pop)

static wavHeader_t wavHeader =
		{ .riff = { .id = "RIFF", .size = 0 }, .format = "WAVE", .fmt = { .id =
				"fmt ", .size = sizeof(wavFormat_t) },
				.wavFormat = { .format = PCM_FORMAT, .numberOfChannels = 1,
						.samplesPerSecond = 0, .bytesPerSecond = 0,
						.bytesPerCapture = 2, .bitsPerSample = 16 }, .list = {
						.id = "LIST", .size = RIFF_ID_LENGTH + sizeof(icmt_t)
								+ sizeof(iart_t) }, .info = "INFO", .icmt = {
						.icmt.id = "ICMT", .icmt.size = LENGTH_OF_COMMENT,
						.comment = "" }, .iart = { .iart.id = "IART",
						.iart.size = LENGTH_OF_ARTIST, .artist = "" }, .data = {
						.id = "data", .size = 0 } };

void setHeaderDetails(uint32_t sampleRate, uint32_t numberOfSamples) {

	wavHeader.wavFormat.samplesPerSecond = sampleRate;
	wavHeader.wavFormat.bytesPerSecond = 2 * sampleRate;
	wavHeader.data.size = 2 * numberOfSamples;
	wavHeader.riff.size = 2 * numberOfSamples + sizeof(wavHeader_t)
			- sizeof(chunk_t);

}

void setHeaderComment(uint32_t currentTime, int8_t timezoneHours,
		int8_t timezoneMinutes, uint8_t *serialNumber, uint32_t gain,
		AM_batteryState_t batteryState, bool batteryVoltageLow,
		bool switchPositionChanged) {

	time_t rawtime = currentTime + timezoneHours * SECONDS_IN_HOUR
			+ timezoneMinutes * SECONDS_IN_MINUTE;

	struct tm *time = gmtime(&rawtime);

	/* Format artist field */

	char *artist = wavHeader.iart.artist;

	sprintf(artist, "AudioMoth %08X%08X",
			(unsigned int) *((uint32_t*) serialNumber + 1),
			(unsigned int) *((uint32_t*) serialNumber));

	/* Format comment field */

	char *comment = wavHeader.icmt.comment;

	sprintf(comment, "Recorded at %02d:%02d:%02d %02d/%02d/%04d (UTC",
			time->tm_hour, time->tm_min, time->tm_sec, time->tm_mday,
			1 + time->tm_mon, 1900 + time->tm_year);

	comment += 36;

	if (timezoneHours < 0)
		sprintf(comment, "%d", timezoneHours);

	if (timezoneHours > 0)
		sprintf(comment, "+%d", timezoneHours);

	if (timezoneHours < 0 || timezoneHours > 0)
		comment += 2;

	if (timezoneHours < -9 || timezoneHours > 9)
		comment += 1;

	if (timezoneMinutes < 0)
		sprintf(comment, ":%2d", -timezoneMinutes);

	if (timezoneMinutes > 0)
		sprintf(comment, ":%2d", timezoneMinutes);

	if (timezoneMinutes < 0 || timezoneMinutes > 0)
		comment += 3;

	sprintf(comment, ") by %s at gain setting %d while battery state was ",
			artist, (unsigned int) gain);

	comment += 74;

	if (batteryState == AM_BATTERY_LOW) {

		sprintf(comment, "less than 3.6V.");

		comment += 15;

	} else if (batteryState >= AM_BATTERY_FULL) {

		sprintf(comment, "greater than 4.9V.");

		comment += 18;

	} else {

		batteryState += 35;

		sprintf(comment, "%01d.%01dV.", batteryState / 10, batteryState % 10);

		comment += 5;

	}

	if (batteryVoltageLow || switchPositionChanged) {

		sprintf(comment, " Recording cancelled before completion due to ");

		comment += 46;

		if (batteryVoltageLow) {

			sprintf(comment, "low battery voltage.");

		} else if (switchPositionChanged) {

			sprintf(comment, "change of switch position.");

		}

	}

}

/* USB configuration data structure */

#pragma pack(push, 1)

typedef struct {
	uint16_t startMinutes;
	uint16_t stopMinutes;
} startStopPeriod_t;

typedef struct {
	uint32_t time;
	uint8_t gain;
	uint8_t clockDivider;
	uint8_t acquisitionCycles;
	uint8_t oversampleRate;
	uint32_t sampleRate;
	uint8_t sampleRateDivider;
	uint16_t sleepDuration;
	uint16_t recordDuration;
	uint8_t enableLED;
	uint8_t activeStartStopPeriods;
	startStopPeriod_t startStopPeriods[MAX_START_STOP_PERIODS];
	int8_t timezoneHours;
	uint8_t enableBatteryCheck;
	uint8_t disableBatteryLevelDisplay;
	int8_t timezoneMinutes;
} configSettings_t;

#pragma pack(pop)

static const configSettings_t defaultConfigSettings = { .time = 0, .gain = 2,
		.clockDivider = 4, .acquisitionCycles = 16, .oversampleRate = 1,
		.sampleRate = 384000, .sampleRateDivider = 8, .sleepDuration = 0,
		.recordDuration = 60, .enableLED = 1, .activeStartStopPeriods = 0,
		.startStopPeriods = { { .startMinutes = 60, .stopMinutes = 120 }, {
				.startMinutes = 300, .stopMinutes = 420 }, {
				.startMinutes = 540, .stopMinutes = 600 }, {
				.startMinutes = 720, .stopMinutes = 780 }, {
				.startMinutes = 900, .stopMinutes = 960 } }, .timezoneHours = 0,
		.enableBatteryCheck = 0, .disableBatteryLevelDisplay = 0,
		.timezoneMinutes = 0 };

static uint32_t *previousSwitchPosition =
		(uint32_t*) AM_BACKUP_DOMAIN_START_ADDRESS;

static uint32_t *timeOfNextRecording =
		(uint32_t*) (AM_BACKUP_DOMAIN_START_ADDRESS + 4);

static uint32_t *durationOfNextRecording =
		(uint32_t*) (AM_BACKUP_DOMAIN_START_ADDRESS + 8);

static configSettings_t *configSettings =
		(configSettings_t*) (AM_BACKUP_DOMAIN_START_ADDRESS + 12);

/* DC filter variables */

static int8_t bitsToShift;

static int32_t previousSample;
static int32_t previousFilterOutput;

/* SRAM buffer variables */

static volatile uint32_t writeBuffer;
static volatile uint32_t writeBufferIndex;

static int16_t* buffers[NUMBER_OF_BUFFERS];

/* Recording state */

static volatile bool switchPositionChanged;

/* DMA buffers */

static int16_t primaryBuffer[NUMBER_OF_SAMPLES_IN_DMA_TRANSFER];
static int16_t secondaryBuffer[NUMBER_OF_SAMPLES_IN_DMA_TRANSFER];

/* Current recording file name */

static char fileName[20];

/* Firmware version and description */

static uint8_t firmwareVersion[AM_FIRMWARE_VERSION_LENGTH] = { 1, 0, 0 };

static uint8_t firmwareDescription[AM_FIRMWARE_DESCRIPTION_LENGTH] =
		"AudioMoth-Firmware-SPL";

/* dBA filter */

static float fRec0[3],fRec1[2],fRec2[2],fRec3[3];

static float a1[2],b1[3],a2,b2[2],a3,b3[2],a4[2],b4[3];

static float w1,w2,w3,w4;

static float spl;
static uint32_t fs;
static uint32_t n;

static char logFilename[20];
static char logBuffer[LOG_BUFFER_LENGTH];

/* Microphone compensation filter */
float fRec0_comp[2];
float fRec1_comp[2];
float a_comp;
float b_comp;
float G_comp;

/* Function prototypes */

static void flashLedToIndicateBatteryLife(void);
static void filter(int16_t *source, int16_t *dest, uint8_t sampleRateDivider,
		uint32_t size);
static void scheduleRecording(uint32_t currentTime,
		uint32_t *timeOfNextRecording, uint32_t *durationOfNextRecording);
static AM_recordingState_t makeRecording(uint32_t currentTime,
		uint32_t recordDuration, bool enableLED, AM_batteryState_t batteryState);

/* Functions of copy to and from the backup domain */

static void copyFromBackupDomain(uint8_t *dst, uint32_t *src, uint32_t length) {

	for (uint32_t i = 0; i < length; i += 1) {
		*(dst + i) = *((uint8_t*) src + i);
	}

}

static void copyToBackupDomain(uint32_t *dst, uint8_t *src, uint32_t length) {

	uint32_t value = 0;

	for (uint32_t i = 0; i < length / 4; i += 1) {
		*(dst + i) = *((uint32_t*) src + i);
	}

	for (uint32_t i = 0; i < length % 4; i += 1) {
		value = (value << 8) + *(src + length - 1 - i);
	}

	*(dst + length / 4) = value;

}

static void reset_dBA_filter() {
	for (int l0 = 0; (l0 < 3); l0 = (l0 + 1)) {
		fRec0[l0] = 0.0f;
		fRec3[l0] = 0.0f;
	}
	for (int l1 = 0; (l1 < 2); l1 = (l1 + 1)) {
		fRec1[l1] = 0.0f;
		fRec2[l1] = 0.0f;
	}
}

// compensation filter
static void reset_compensation_filter() {
	for (int ix = 0; (ix < 2); ix = (ix + 1)) {
		fRec0_comp[ix] = 0.0f;
		fRec1_comp[ix] = 0.0f;
	}
}

static void init_compensation_filter() {
	reset_compensation_filter();

	fs = configSettings->sampleRate / configSettings->sampleRateDivider;

/*	if (fs == 8000) {
		a_comp = -0.479608;
		b_comp = -0.356511;
		G_comp = 0.9836;
	} else if (fs == 16000) {
		a_comp = -0.560221;
		b_comp = -0.380794;
		G_comp = 0.874524;
	} else if (fs == 32000) {
		a_comp = -0.751971;
		b_comp = -0.319679;
		G_comp = 0.666271;
	} else if (fs == 48000) {
		a_comp = -0.761089;
		b_comp = -0.0525575;
		G_comp = 0.496156;
	} else {
		a_comp = 0.0;
		b_comp = 0.0;
		G_comp = 1.0;
	}*/

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


static void init_dba_filter() {

	spl = 0.0f;
	n = 0;
	reset_dBA_filter();

	float f1 = 20.6f;
	float f2 = 107.7f;
	float f3 = 737.9f;
	float f4 = 12194.0f;

    fs = configSettings->sampleRate / configSettings->sampleRateDivider;

    w1 = 2.0f*PI*f1;
    w2 = 2.0f*PI*f2;
    w3 = 2.0f*PI*f3;
    w4 = 2.0f*PI*f4;

	float a0;
    a0 = pow((w1 + 2.0f*fs),2);
	a1[0] = 2.0f*(pow(w1,2)-4.0f*pow(fs,2))/a0;
	a1[1] = pow(w1-2.0f*fs,2)/a0;
	b1[0] = 2.0f*fs/a0;
	b1[1] = 0.0f;
	b1[2] = -2.0f*fs/a0;

	a0 = w2 + 2.0f*fs;
	b2[0] = 2.0f*fs/a0;
	b2[1] = -2.0f*fs/a0;
	a2 = (w2-2.0f*fs)/a0;

	a0 = w3 + 2.0f*fs;
	b3[0] = 2.0f*fs/a0;
	b3[1] = -2.0f*fs/a0;
	a3 = (w3-2.0f*fs)/a0;

    a0 = pow((w4 + 2.0f*fs),2);
	a4[0] = 2.0f*(pow(w4,2)-4.0f*pow(fs,2))/a0;
	a4[1] = pow(w4-2.0f*fs,2)/a0;
	b4[0] = 2.0f*fs/a0;
	b4[1] = 0.0f;
	b4[2] = -2.0f*fs/a0;

	sprintf(logFilename, "spl%01d.log", 0);
}

/* Main function */
int main(void) {

	/* Initialise device */

	AudioMoth_initialise();

	/* Init dBA filter variables */
	init_dba_filter();

	/* init compensation filter */
	init_compensation_filter();

	AM_switchPosition_t switchPosition = AudioMoth_getSwitchPosition();

	if (AudioMoth_isInitialPowerUp()) {

		*timeOfNextRecording = 0;

		*durationOfNextRecording = 0;

		*previousSwitchPosition = AM_SWITCH_NONE;

		copyToBackupDomain((uint32_t*) configSettings,
				(uint8_t*) &defaultConfigSettings, sizeof(configSettings_t));

	} else {

		/* Indicate battery state is not initial power up and switch has been moved into USB if enabled */

		if (switchPosition != *previousSwitchPosition
				&& switchPosition == AM_SWITCH_USB
				&& !configSettings->disableBatteryLevelDisplay) {

			flashLedToIndicateBatteryLife();

		}

	}

	/* Handle the case that the switch is in USB position  */

	if (switchPosition == AM_SWITCH_USB) {

		AudioMoth_handleUSB();

		SAVE_SWITCH_POSITION_AND_POWER_DOWN(DEFAULT_WAIT_INTERVAL);

	}

	/* Handle the case that the switch is in CUSTOM position but the time has not been set */

	if (switchPosition == AM_SWITCH_CUSTOM
			&& (AudioMoth_hasTimeBeenSet() == false
					|| configSettings->activeStartStopPeriods == 0)) {

		FLASH_LED(Both, SHORT_LED_FLASH_DURATION)

		SAVE_SWITCH_POSITION_AND_POWER_DOWN(DEFAULT_WAIT_INTERVAL);

	}

	/* Calculate time of next recording if switch has changed position */

	uint32_t currentTime;

	AudioMoth_getTime(&currentTime, NULL);

	if (switchPosition != *previousSwitchPosition) {

		if (switchPosition == AM_SWITCH_DEFAULT) {

			/* Set parameters to start recording now */

			*timeOfNextRecording = currentTime;

			*durationOfNextRecording = configSettings->recordDuration;

		} else {

			/* Determine starting time and duration of next recording */

			scheduleRecording(currentTime, timeOfNextRecording,
					durationOfNextRecording);

		}

	}

	/* Make recording if appropriate */

	bool enableLED = (switchPosition == AM_SWITCH_DEFAULT)
			|| configSettings->enableLED;

	if (currentTime >= *timeOfNextRecording) {

		/* Make recording is battery check is disabled or enabled and okay */

		AM_recordingState_t recordingState = RECORDING_OKAY;

		AM_batteryState_t batteryState = AudioMoth_getBatteryState();

		if (!configSettings->enableBatteryCheck
				|| batteryState > AM_BATTERY_LOW) {

			recordingState = makeRecording(currentTime,
					*durationOfNextRecording, enableLED, batteryState);

		} else if (enableLED) {

			FLASH_LED(Both, LONG_LED_FLASH_DURATION);

		}

		/* Schedule next recording */

		if (switchPosition == AM_SWITCH_DEFAULT) {

			/* Set parameters to start recording after sleep period */

			if (recordingState != SWITCH_CHANGED) {

				*timeOfNextRecording = currentTime
						+ configSettings->recordDuration
						+ configSettings->sleepDuration;

			}

		} else {

			/* Determine starting time and duration of next recording */

			scheduleRecording(currentTime, timeOfNextRecording,
					durationOfNextRecording);

		}

	} else if (enableLED) {

		/* Flash LED to indicate waiting */

		FLASH_LED(Green, WAITING_LED_FLASH_DURATION);

	}

	/* Determine how long to power down */

	uint32_t secondsToSleep = 0;

	if (*timeOfNextRecording > currentTime) {

		secondsToSleep = MIN(*timeOfNextRecording - currentTime,
				WAITING_LED_FLASH_INTERVAL);

	}

	/* Power down */

	SAVE_SWITCH_POSITION_AND_POWER_DOWN(secondsToSleep);

}

/* Time zone handler */

inline void AudioMoth_timezoneRequested(int8_t *timezoneHours,
		int8_t *timezoneMinutes) {

	*timezoneHours = configSettings->timezoneHours;

	*timezoneMinutes = configSettings->timezoneMinutes;

}

/* AudioMoth interrupt handlers */

inline void AudioMoth_handleSwitchInterrupt() {

	switchPositionChanged = true;

}

inline void AudioMoth_handleMicrophoneInterrupt(int16_t sample) {
}

inline void AudioMoth_handleDirectMemoryAccessInterrupt(bool isPrimaryBuffer,
		int16_t **nextBuffer) {

	int16_t *source = secondaryBuffer;

	if (isPrimaryBuffer)
		source = primaryBuffer;

	/* Update the current buffer index and write buffer */

	filter(source, buffers[writeBuffer] + writeBufferIndex,
			configSettings->sampleRateDivider,
			NUMBER_OF_SAMPLES_IN_DMA_TRANSFER);

	writeBufferIndex += NUMBER_OF_SAMPLES_IN_DMA_TRANSFER
			/ configSettings->sampleRateDivider;

	if (writeBufferIndex == NUMBER_OF_SAMPLES_IN_BUFFER) {

		writeBufferIndex = 0;

		writeBuffer = (writeBuffer + 1) & (NUMBER_OF_BUFFERS - 1);

	}

}

/* AudioMoth USB message handlers */

inline void AudioMoth_usbFirmwareVersionRequested(uint8_t **firmwareVersionPtr) {

	*firmwareVersionPtr = firmwareVersion;

}

inline void AudioMoth_usbFirmwareDescriptionRequested(
		uint8_t **firmwareDescriptionPtr) {

	*firmwareDescriptionPtr = firmwareDescription;

}

inline void AudioMoth_usbApplicationPacketRequested(uint32_t messageType,
		uint8_t *transmitBuffer, uint32_t size) {

	/* Copy the current time to the USB packet */

	uint32_t currentTime;

	AudioMoth_getTime(&currentTime, NULL);

	memcpy(transmitBuffer + 1, &currentTime, 4);

	/* Copy the unique ID to the USB packet */

	memcpy(transmitBuffer + 5, (uint8_t*) AM_UNIQUE_ID_START_ADDRESS,
			AM_UNIQUE_ID_SIZE_IN_BYTES);

	/* Copy the battery state to the USB packet */

	AM_batteryState_t batteryState = AudioMoth_getBatteryState();

	memcpy(transmitBuffer + 5 + AM_UNIQUE_ID_SIZE_IN_BYTES, &batteryState, 1);

	/* Copy the firmware version to the USB packet */

	memcpy(transmitBuffer + 6 + AM_UNIQUE_ID_SIZE_IN_BYTES, firmwareVersion,
			AM_FIRMWARE_VERSION_LENGTH);

	/* Copy the firmware description to the USB packet */

	memcpy(
			transmitBuffer + 6 + AM_UNIQUE_ID_SIZE_IN_BYTES
					+ AM_FIRMWARE_VERSION_LENGTH, firmwareDescription,
			AM_FIRMWARE_DESCRIPTION_LENGTH);

}

inline void AudioMoth_usbApplicationPacketReceived(uint32_t messageType,
		uint8_t* receiveBuffer, uint8_t *transmitBuffer, uint32_t size) {

	/* Copy the USB packet contents to the back-up register data structure location */

	copyToBackupDomain((uint32_t*) configSettings, receiveBuffer + 1,
			sizeof(configSettings_t));

	/* Copy the back-up register data structure to the USB packet */

	copyFromBackupDomain(transmitBuffer + 1, (uint32_t*) configSettings,
			sizeof(configSettings_t));

	/* Set the time */

	AudioMoth_setTime(configSettings->time, 0);

}

/* convert float number to string */
static void float_to_string(char* string, float value) {
	char *tmpSign = (value < 0) ? "-" : "";
	float tmpVal = (value < 0) ? -value : value;

	int tmpInt1 = tmpVal;                  // Get the integer (678).
	float tmpFrac = tmpVal - tmpInt1;      // Get fraction (0.0123).
	int tmpInt2 = trunc(tmpFrac * 10000);  // Turn into integer (123).

	sprintf(string, "%s%d.%04d ",tmpSign, tmpInt1, tmpInt2);
}

/* Append message (spl value) to logfile */
static void writeSPL_log(uint32_t currentTime, float value) { // char *message

	AudioMoth_enableFileSystem();

	AudioMoth_appendFile(logFilename);
	//AudioMoth_openFile(logFilename);
	struct tm *time = gmtime((time_t*)&currentTime);

	sprintf(logBuffer, "%02d/%02d/%04d %02d:%02d:%02d: ", time->tm_mday, time->tm_mon + 1, time->tm_year + 1900, time->tm_hour, time->tm_min, time->tm_sec);

	AudioMoth_writeToFile(logBuffer, strnlen(logBuffer, LOG_BUFFER_LENGTH));

	float_to_string(logBuffer, value);
	AudioMoth_writeToFile(logBuffer, strnlen(logBuffer, LOG_BUFFER_LENGTH));

	AudioMoth_writeToFile("\n", 1);

	AudioMoth_closeFile();
}


/*static float compensation_mic_filter_step(float sample) {
	float filtered_sample;
	fRec0_comp[0] = (sample - (a_comp * fRec0_comp[1]));
	filtered_sample = (float)(G_comp * (fRec0_comp[0] + (b_comp * fRec0_comp[1])));
	fRec0_comp[1] = fRec0_comp[0];
	return filtered_sample;
}*/

static float compensation_mic_filter_step(float sample) {
	float filtered_sample;
	fRec1_comp[0] = (sample - (a_comp * fRec1_comp[1]));
	fRec0_comp[0] = ((fRec1_comp[0] + (b_comp * fRec1_comp[1])) - (a_comp * fRec0_comp[1]));
	filtered_sample = (float) (G_comp * (fRec0_comp[0] + (b_comp * fRec0_comp[1])));
	fRec1_comp[1] = fRec1_comp[0];
	fRec0_comp[1] = fRec0_comp[0];
	return filtered_sample;
}



static float dBA_filter_step(float sample) {
	float filteredOutput_A;
	fRec3[0] = (sample - ((a1[0] * fRec3[1]) + (a1[1] * fRec3[2])));
	fRec2[0] = ((((b1[0] * fRec3[0]) + (b1[1] * fRec3[1])) + (b1[2] * fRec3[2])) - (a2 * fRec2[1]));
	fRec1[0] = (((b2[0] * fRec2[0]) + (b2[1] * fRec2[1])) - (a3 * fRec1[1]));
	fRec0[0] = (((b3[0] * fRec1[0]) + (b3[1] * fRec1[1])) - ((a4[0] * fRec0[1]) + (a4[1] * fRec0[2])));
	filteredOutput_A = (float)((GA * w4*w4 * (((b4[0] * fRec0[0]) + (b4[1] * fRec0[1])) + (b4[2] * fRec0[2]))));

	/* uncomment to save the A weighted signal*/
	//filteredOutput = const_normalize * filteredOutput_A;

	fRec3[2] = fRec3[1];
	fRec3[1] = fRec3[0];
	fRec2[1] = fRec2[0];
	fRec1[1] = fRec1[0];
	fRec0[2] = fRec0[1];
	fRec0[1] = fRec0[0];

	return filteredOutput_A;
}

/* Remove DC offset from the microphone samples */
/* Edited to implement dbA filter */
static void filter(int16_t *source, int16_t *dest, uint8_t sampleRateDivider,
		uint32_t size) {

	int32_t filteredOutput;
	float filteredOutput_A;
	int32_t scaledPreviousFilterOutput;

	int index = 0;

	for (int i = 0; i < size; i += sampleRateDivider) {

		int32_t sample = 0;

		for (int j = 0; j < sampleRateDivider; j += 1) {

			sample += (int32_t) source[i + j];

		}

		if (bitsToShift > 0)
			sample <<= bitsToShift;

		if (bitsToShift < 0)
			sample >>= -bitsToShift;

		scaledPreviousFilterOutput = (int32_t) (DC_BLOCKING_FACTOR
				* (float) previousFilterOutput);

		filteredOutput = sample - previousSample + scaledPreviousFilterOutput;

		/* uncomment to save original signal*/
		//filteredOutput = sample;

		float const_normalize = 3276.8f;
		float input;// = (float)sample / const_normalize;

		input = compensation_mic_filter_step((float)sample / const_normalize);
		filteredOutput_A = dBA_filter_step(input);

		/* uncomment to save the A weighted signal*/
		//filteredOutput = const_normalize * filteredOutput_A;

		spl = (n*spl + filteredOutput_A*filteredOutput_A)/(n+1); // y[n] = (n*y[n]+x[n])/(n+1)

		n += 1;

		if (filteredOutput > INT16_MAX) {

			dest[index++] = INT16_MAX;

		} else if (filteredOutput < INT16_MIN) {

			dest[index++] = INT16_MIN;

		} else {

			dest[index++] = (int16_t) filteredOutput;

		}

		previousFilterOutput = filteredOutput;

		previousSample = sample;

	}

}






/* Save recording to SD card */

static AM_recordingState_t makeRecording(uint32_t currentTime,
		uint32_t recordDuration, bool enableLED, AM_batteryState_t batteryState) {

	/* Initialise buffers */

	writeBuffer = 0;

	writeBufferIndex = 0;

	buffers[0] = (int16_t*) AM_EXTERNAL_SRAM_START_ADDRESS;

	for (int i = 1; i < NUMBER_OF_BUFFERS; i += 1) {
		buffers[i] = buffers[i - 1] + NUMBER_OF_SAMPLES_IN_BUFFER;
	}

	/* Calculate the bits to shift */

	bitsToShift = 0;

	uint16_t oversampling = configSettings->oversampleRate
			* configSettings->sampleRateDivider;

	while (oversampling > 16) {
		oversampling >>= 1;
		bitsToShift -= 1;
	}

	while (oversampling < 16) {
		oversampling <<= 1;
		bitsToShift += 1;
	}

	/* Calculate recording parameters */

	uint32_t numberOfSamplesInHeader = sizeof(wavHeader) >> 1;

	uint32_t numberOfSamples = configSettings->sampleRate
			/ configSettings->sampleRateDivider * recordDuration;

	/* Enable the battery monitor */

	if (configSettings->enableBatteryCheck) {

		AudioMoth_enableBatteryMonitor();

		AudioMoth_setBatteryMonitorThreshold(AM_BATTERY_LOW);

	}

	/* Initialise microphone for recording */

	AudioMoth_enableExternalSRAM();

	AudioMoth_enableMicrophone(configSettings->gain,
			configSettings->clockDivider, configSettings->acquisitionCycles,
			configSettings->oversampleRate);

	AudioMoth_initialiseDirectMemoryAccess(primaryBuffer, secondaryBuffer,
			NUMBER_OF_SAMPLES_IN_DMA_TRANSFER);

	AudioMoth_startMicrophoneSamples(configSettings->sampleRate);

	/* Initialise file system and open a new file */

	if (enableLED) {

		AudioMoth_setRedLED(true);

	}

	RETURN_ON_ERROR(AudioMoth_enableFileSystem());

	/* Open a file with the current local time as the name */

	time_t rawtime = currentTime
			+ configSettings->timezoneHours * SECONDS_IN_HOUR
			+ configSettings->timezoneMinutes * SECONDS_IN_MINUTE;

	struct tm *time = gmtime(&rawtime);

	sprintf(fileName, "%04d%02d%02d_%02d%02d%02d.WAV", 1900 + time->tm_year,
			time->tm_mon + 1, time->tm_mday, time->tm_hour, time->tm_min,
			time->tm_sec);

	RETURN_ON_ERROR(AudioMoth_openFile(fileName));

	AudioMoth_setRedLED(false);

	/* Termination conditions */

	switchPositionChanged = false;

	bool batteryVoltageLow = false;

	/* Main record loop */

	uint32_t samplesWritten = 0;

	uint32_t buffersProcessed = 0;

	uint32_t readBuffer = writeBuffer;

	while (samplesWritten < numberOfSamples + numberOfSamplesInHeader
			&& !switchPositionChanged && !batteryVoltageLow) {

		while (readBuffer != writeBuffer
				&& samplesWritten < numberOfSamples + numberOfSamplesInHeader
				&& !switchPositionChanged && !batteryVoltageLow) {

			/* Light LED during SD card write if appropriate */

			if (enableLED) {

				AudioMoth_setRedLED(true);

			}

			/* Write the appropriate number of bytes to the SD card */

			uint32_t numberOfSamplesToWrite = 0;

			if (buffersProcessed >= NUMBER_OF_BUFFERS_TO_SKIP) {

				numberOfSamplesToWrite = MIN(
						numberOfSamples + numberOfSamplesInHeader
								- samplesWritten, NUMBER_OF_SAMPLES_IN_BUFFER);

			}

			RETURN_ON_ERROR(
					AudioMoth_writeToFile(buffers[readBuffer],
							2 * numberOfSamplesToWrite));

			/* Increment buffer counters */

			readBuffer = (readBuffer + 1) & (NUMBER_OF_BUFFERS - 1);

			samplesWritten += numberOfSamplesToWrite;

			buffersProcessed += 1;

			/* Clear LED */

			AudioMoth_setRedLED(false);

		}

		/* Check the battery level */

		if (configSettings->enableBatteryCheck
				&& !AudioMoth_isBatteryMonitorAboveThreshold()) {

			batteryVoltageLow = true;

		}

		/* Sleep until next DMA transfer is complete */

		AudioMoth_sleep();

	}

	/* Disable battery check */

	if (configSettings->enableBatteryCheck) {

		AudioMoth_disableBatteryMonitor();

	}

	/* Initialise the WAV header */

	samplesWritten = MAX(numberOfSamplesInHeader, samplesWritten);

	setHeaderDetails(
			configSettings->sampleRate / configSettings->sampleRateDivider,
			samplesWritten - numberOfSamplesInHeader);

	setHeaderComment(currentTime, configSettings->timezoneHours,
			configSettings->timezoneMinutes,
			(uint8_t*) AM_UNIQUE_ID_START_ADDRESS, configSettings->gain,
			batteryState, batteryVoltageLow, switchPositionChanged);

	/* Write the header */

	if (enableLED) {

		AudioMoth_setRedLED(true);

	}

	RETURN_ON_ERROR(AudioMoth_seekInFile(0));

	RETURN_ON_ERROR(AudioMoth_writeToFile(&wavHeader, sizeof(wavHeader)));

	/* Close the file */

	RETURN_ON_ERROR(AudioMoth_closeFile());

	AudioMoth_setRedLED(false);

	/* Return with state */

	if (batteryVoltageLow)
		return BATTERY_CHECK;

	if (switchPositionChanged)
		return SWITCH_CHANGED;

	/* Save SPL value to log file*/

	float cal_offset;

	switch (configSettings->gain) {
	    case 0: cal_offset = CALdBA_low; break;
	    case 1: cal_offset = CALdBA_low_med; break;
	    case 2: cal_offset = CALdBA_med; break;
	    case 3: cal_offset = CALdBA_med_high; break;
	    case 4: cal_offset = CALdBA_high; break;
	}

	spl = 10.0f*log10f(spl) + cal_offset;
	writeSPL_log(currentTime, spl);

	/* reset filters */
	spl = 0.0f;
	n = 0;
	reset_dBA_filter();
	reset_compensation_filter();

	return RECORDING_OKAY;

}

static void scheduleRecording(uint32_t currentTime,
		uint32_t *timeOfNextRecording, uint32_t *durationOfNextRecording) {

	/* Check number of active state stop periods */

	uint32_t activeStartStopPeriods = MIN(
			configSettings->activeStartStopPeriods, MAX_START_STOP_PERIODS);

	/* No active periods */

	if (activeStartStopPeriods == 0) {

		*timeOfNextRecording = UINT32_MAX;

		*durationOfNextRecording = configSettings->recordDuration;

		return;

	}

	/* Calculate the number of seconds of this day */

	time_t rawtime = currentTime;

	struct tm *time = gmtime(&rawtime);

	uint32_t currentSeconds = SECONDS_IN_HOUR * time->tm_hour
			+ SECONDS_IN_MINUTE * time->tm_min + time->tm_sec;

	/* Check each active start stop period */

	uint32_t durationOfCycle = configSettings->recordDuration
			+ configSettings->sleepDuration;

	for (uint32_t i = 0; i < activeStartStopPeriods; i += 1) {

		startStopPeriod_t *period = configSettings->startStopPeriods + i;

		/* Calculate the start and stop time of the current period */

		uint32_t startSeconds = SECONDS_IN_MINUTE * period->startMinutes;

		uint32_t stopSeconds = SECONDS_IN_MINUTE * period->stopMinutes;

		/* Calculate time to next period or time to next start in this period */

		if (currentSeconds < startSeconds) {

			*timeOfNextRecording = currentTime
					+ (startSeconds - currentSeconds);

			*durationOfNextRecording = MIN(configSettings->recordDuration,
					stopSeconds - startSeconds);

			return;

		} else if (currentSeconds < stopSeconds) {

			uint32_t cycles = (currentSeconds - startSeconds + durationOfCycle)
					/ durationOfCycle;

			uint32_t secondsFromStartOfPeriod = cycles * durationOfCycle;

			if (secondsFromStartOfPeriod < stopSeconds - startSeconds) {

				*timeOfNextRecording = currentTime
						+ (startSeconds - currentSeconds)
						+ secondsFromStartOfPeriod;

				*durationOfNextRecording = MIN(configSettings->recordDuration,
						stopSeconds - startSeconds - secondsFromStartOfPeriod);

				return;

			}

		}

	}

	/* Calculate time until first period tomorrow */

	startStopPeriod_t *firstPeriod = configSettings->startStopPeriods;

	uint32_t startSeconds = SECONDS_IN_MINUTE * firstPeriod->startMinutes;

	uint32_t stopSeconds = SECONDS_IN_MINUTE * firstPeriod->stopMinutes;

	*timeOfNextRecording = currentTime + (SECONDS_IN_DAY - currentSeconds)
			+ startSeconds;

	*durationOfNextRecording = MIN(configSettings->recordDuration,
			stopSeconds - startSeconds);

}

static void flashLedToIndicateBatteryLife(void) {

	uint32_t numberOfFlashes = LOW_BATTERY_LED_FLASHES;

	AM_batteryState_t batteryState = AudioMoth_getBatteryState();

	/* Set number of flashes according to battery state */

	if (batteryState > AM_BATTERY_LOW) {

		numberOfFlashes = (batteryState >= AM_BATTERY_4V6) ? 4 :
							(batteryState >= AM_BATTERY_4V4) ? 3 :
							(batteryState >= AM_BATTERY_4V0) ? 2 : 1;

	}

	/* Flash LED */

	for (uint32_t i = 0; i < numberOfFlashes; i += 1) {

		FLASH_LED(Red, SHORT_LED_FLASH_DURATION)

		if (numberOfFlashes == LOW_BATTERY_LED_FLASHES) {

			AudioMoth_delay(SHORT_LED_FLASH_DURATION);

		} else {

			AudioMoth_delay(LONG_LED_FLASH_DURATION);

		}

	}

}

