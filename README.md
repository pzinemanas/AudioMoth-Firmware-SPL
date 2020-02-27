# AudioMoth-Firmware-SPL
This repository contains a firmware for AudioMoth. It consits in an adaptation of the firmware publicated in [AudioMoth-Project](https://github.com/OpenAcousticDevices/AudioMoth-Project) and [AudioMoth-Firmware-Basic](https://github.com/OpenAcousticDevices/AudioMoth-Firmware-Basic) to calculate SPL (dBA). 

We use the 1.3.0 version of AudioMoth firmware as a base. We add the SPL library (`src/spl.c` and inc/spl.h) that includes all the functions related to the SPL estimation. 

The main changes are in the [filter](https://github.com/OpenAcousticDevices/AudioMoth-Firmware-Basic/blob/master/main.c#L609) function of the 1.3.0 version of firmware. In this function, we add the implementation of two real-time filters to compensate the microphone frequency response ant to apply the A-weighting to the signal. 

## Microphone response compensation
In order to have an almost flat microphone's frequency response, we implement an IIR filter that compensates the response in the low frequencies. In the near future, we are going to do some experiments to better do this compensation. See [Mic_compensation_filter](https://github.com/pzinemanas/AudioMoth-Firmware-SPL/blob/master/notebooks/Mic_compensation_filter.ipynb) notebook for more details about filter design process.

## A-weighting

Note that unlike other methods based on the frequency domain, we apply the weighting on the time-domain of the signal. See [A_weighting_filter](https://github.com/pzinemanas/AudioMoth-Firmware-SPL/blob/master/notebooks/A_weighting_filter.ipynb) notebook for more details.

## Flashing this firmware to Audiomoth
To flash this firmware to the AudioMoth, follow the instructions from the [OpenAcoustic team](https://github.com/OpenAcousticDevices/Flash). 

## Editing this firmware
To edit this firmware, clone this repository and follow the instructions from the [AudioMoth wiki](https://github.com/OpenAcousticDevices/AudioMoth-Project/wiki/AudioMoth). 

## Organization of the repository

/src --> includes the C sources of the firmware

/bin --> includes the compiled firmware ready to [flash](https://github.com/OpenAcousticDevices/Flash) to AudioMoth

/notebooks --> includes jupyter noteboks with the filter design process, calibration among others.

Other folders are includes related to the AudioMoth original firmware.

## Licence


