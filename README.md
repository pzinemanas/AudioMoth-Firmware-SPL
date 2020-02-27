# AudioMoth-Firmware-SPL
This repository contains a firmware for AudioMoth. It consits in an adaptation of the firmware publicated in [AudioMoth-Project](https://github.com/OpenAcousticDevices/AudioMoth-Project) and [AudioMoth-Firmware-Basic](https://github.com/OpenAcousticDevices/AudioMoth-Firmware-Basic) to calculate SPL (dBA). 

We use the 1.3.0 version of AudioMoth firmware as a base. We add the SPL library (`src/spl.c` and `inc/spl.h`) that includes all the functions related to the SPL estimation. 

The main modifications of the `src/main.c` file of [AudioMoth-Firmware-Basic](https://github.com/OpenAcousticDevices/AudioMoth-Firmware-Basic) are in the [filter](https://github.com/OpenAcousticDevices/AudioMoth-Firmware-Basic/blob/master/main.c#L609). In this function, we add the call of the microphone frequency response compensation and the A-weighting filter of the signal. 

## Singal processing
### Microphone response compensation
In order to have an almost flat microphone's frequency response, we implement an IIR filter that compensates the response in the low frequencies. In the near future, we are going to do some experiments to better do this compensation. See [Mic_compensation_filter](https://github.com/pzinemanas/AudioMoth-Firmware-SPL/blob/master/notebooks/Mic_compensation_filter.ipynb) notebook for more details about filter design process.

### A-weighting

Note that unlike other methods based on the frequency domain, we apply the weighting on the time-domain of the signal. See [A_weighting_filter](https://github.com/pzinemanas/AudioMoth-Firmware-SPL/blob/master/notebooks/A_weighting_filter.ipynb) notebook for more details.

## Using this firmware
### Flashing this firmware to Audiomoth
Flash the `bin/AudioMoth-Firmware-SPL.bin` file following the instructions from the [OpenAcoustic team](https://github.com/OpenAcousticDevices/Flash).

### Editing this firmware
To edit this firmware, clone this repository and follow the instructions from the [AudioMoth wiki](https://github.com/OpenAcousticDevices/AudioMoth-Project/wiki/AudioMoth). 

## Organization of the repository


````
AudioMoth-Firmware-SPL/
|
|- src/ _______________________________ # Firmware source files
|  |- main.c __________________________ # Main program (edited from AudioMoth firmware 1.3.0)
|  |- AudiMoth.c ______________________ # AudioMoth library
|  |- spl.c ___________________________ # SPL library
|
|- inc/ _______________________________ # Firmware header files
|  |- AudiMoth.h ______________________ # AudioMoth header
|  |- spl.h ___________________________ # SPL library header
|
|- bin/
|  |- AudioMoth-Firmware-SPL.bin ______ # Compiled firmware ready to AudioMoth
|
|- notebooks/ _________________________ # Jupyter noteboks
|  |- A_weighting_filter.ipynb ________ # Filter desing process of A_weighting filter
|  |- Mic_compensation_filter.ipynb ___ # Filter desing process of compensation filter
````
Other folders are includes related to the AudioMoth original firmware.

## Licence
We use the same licence (MIT) that [AudioMoth-Project](https://github.com/OpenAcousticDevices/AudioMoth-Project), Copyright (c) 2017 OpenAcousticDevices. Files `src/spl.c` and `inc/spl.h` and the modifications of `src/main.c` have Copyright (c) 2020 Pablo Zinemanas.

