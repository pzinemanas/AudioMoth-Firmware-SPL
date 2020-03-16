# AudioMoth-Firmware-SPL
This repository contains an AudioMoth firmware adaptation to calculate the Sound Pressure Level (SPL). This is based on the 1.3.0 version of AudioMoth firmware (published on [AudioMoth-Project](https://github.com/OpenAcousticDevices/AudioMoth-Project) and [AudioMoth-Firmware-Basic](https://github.com/OpenAcousticDevices/AudioMoth-Firmware-Basic)). We include the SPL library (`src/spl.c` and `inc/spl.h`) that implement all the functions related to the SPL estimation. 

The main modifications of the `src/main.c` file of [AudioMoth-Firmware-Basic](https://github.com/OpenAcousticDevices/AudioMoth-Firmware-Basic) are in the [filter](https://github.com/OpenAcousticDevices/AudioMoth-Firmware-Basic/blob/master/main.c#L609) function, where we add the SPL functions calls. 

## SPL calculation

We estimate the SPL using the audio signal recorded by the AudioMoth. We calculate the [LAeq,T](http://www.acoustic-glossary.co.uk/leq.htm) in the time basis, <img src="https://render.githubusercontent.com/render/math?math=T">, defined by the record duration configured in AudioMoth. Therefore, for each wav file recorded in AudioMoth, a SPL value is calculated and saved into a log file in the SD card. Thus, if <img src="https://render.githubusercontent.com/render/math?math=x[n]"> is the signal recorded by AudioMoth, we estimate the SPL by the mean energy:

<img src="https://render.githubusercontent.com/render/math?math=SPL = \frac{1}{N}\sum_{n=0}^{N-1} x_A^2[n]">

where <img src="https://render.githubusercontent.com/render/math?math=N"> is the number of samples in the time <img src="https://render.githubusercontent.com/render/math?math=T">, and <img src="https://render.githubusercontent.com/render/math?math=x_A[n]"> is the [A-weighted](http://www.acoustic-glossary.co.uk/frequency-weighting.htm) signal. Also, as the AudioMoth microphone does not have a flat frequency response, it is necessary to apply a filter to compensate its response. Therefore before we calculate <img src="https://render.githubusercontent.com/render/math?math=x_A[n]">, we apply the filter compensation. The next diagram illustrates the process flow.

<img src="./notebooks/imgs/diagram.png">

## Signal processing

In order to compensate the microphone frequency response and to apply the A-weighting to the signal, we implement different Infinite Impulse Response (IIR) filters. When the filter order is too high, we split it into parts (first or second order filters). Each of this parts is implemented by the [Direct-Form-II (DF-II)](https://ccrma.stanford.edu/~jos/filters/Direct_Form_II.html) realization. We base our implementation in the result of the [faust filter library](http://faust.grame.fr/editor/libraries/doc/library.html#fi.iir).

### Microphone response compensation
In order to have an almost flat microphone frequency response, we implement an IIR filter that compensates the response in the low frequencies. In the near future, we are going to improve this compensation. See [Mic_compensation_filter](https://github.com/pzinemanas/AudioMoth-Firmware-SPL/blob/master/notebooks/Mic_compensation_filter.ipynb) notebook for more details about filter design process.

### A-weighting

Note that unlike other methods based on the frequency domain, we apply the weighting on the time-domain of the signal. See [A_weighting_filter](https://github.com/pzinemanas/AudioMoth-Firmware-SPL/blob/master/notebooks/A_weighting_filter.ipynb) notebook for more details.

## Using this firmware
### Flashing this firmware to Audiomoth
Flash the `bin/AudioMoth-Firmware-SPL.bin` file following the instructions from the [OpenAcoustic team](https://github.com/OpenAcousticDevices/Flash).

### Editing this firmware
To edit this firmware, clone this repository and follow the instructions from the [AudioMoth wiki](https://github.com/OpenAcousticDevices/AudioMoth-Project/wiki/AudioMoth). 

## Organization of the repository

This repository is organized as follows:

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
|  |- A_weighting_filter.ipynb ________ # Filter design process of A_weighting filter
|  |- Mic_compensation_filter.ipynb ___ # Filter design process of compensation filter
````
Other folders are libraries includes in the AudioMoth original firmware.

## Licence
We use the same license (MIT) that [AudioMoth-Project](https://github.com/OpenAcousticDevices/AudioMoth-Project), Copyright (c) 2017 OpenAcousticDevices. Files `src/spl.c` and `inc/spl.h` and the modifications of `src/main.c` have Copyright (c) 2020 Pablo Zinemanas.

