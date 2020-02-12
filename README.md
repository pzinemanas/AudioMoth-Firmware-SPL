# AudioMoth-Firmware-SPL
This repository contains a firmware for AudioMoth. It consits in an adaptation of the firmware publicated in [AudioMoth-Project](https://github.com/OpenAcousticDevices/AudioMoth-Project) and [AudioMoth-Firmware-Basic](https://github.com/OpenAcousticDevices/AudioMoth-Firmware-Basic) to calculate SPL (dBA). 

The main changes are in the [filter](https://github.com/OpenAcousticDevices/AudioMoth-Firmware-Basic/blob/master/main.c#L609) function of the 1.3.0 version of firmware. In this function, we add the implementation of a real-time filter to apply the A-weighting to the signal. Note that unlike other methods based on the frequency domain, we apply the weighting on the time-domain of the signal.

## Organization

/src includes the C sources of the firmware

/bin includes the firmware compiled ready to [flash](https://github.com/OpenAcousticDevices/Flash) to AudioMoth

/notebooks includes jupyter noteboks with the filter design process, calibration among others.

Other folders are includes related to the AudioMoth original firmware.

## Disclaimer

The calibration process was not carried out, thus the SPL mesaure is inexact yet.
