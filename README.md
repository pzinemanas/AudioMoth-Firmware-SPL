# AudioMoth-Firmware-SPL
This repository contains a firmware for AudioMoth. It consits in an adaptation of the firmware publicated in [AudioMoth-Project](https://github.com/OpenAcousticDevices/AudioMoth-Project) and [AudioMoth-Firmware-Basic](https://github.com/OpenAcousticDevices/AudioMoth-Firmware-Basic) to calculate SPL (dBA). 

The main changes are in the [filter](https://github.com/OpenAcousticDevices/AudioMoth-Firmware-Basic/blob/master/main.c#L609) function of the 1.3.0 version of firmware. In this function, we add the implementation of a real-time filter to apply the A-weighting to the signal. Note that unlike other methods based on the frequency domain, we apply the weighting on the time-domain of the signal.

## A-weighting filter implementation

The A-weighting function transform is defined in the ANSI Standards S1.4-1983 and S1.42-2001. We use the bilinear transform to cover the function transform to the  


