# Arduino based WSPR beason
## Intro
Hi, welcome to my WSPR repository. Disclaimer: I'm a hobbyist, you'll find plenty of bugs here or improvements. As long as it is constructive, feel free to propose improvements. 
Or fork this and make your own version. I adapted the sketch from VK3HN to include bandhopping and reduce RAM usage even further. I have no background in CS, and everything is done using Google-fu, especially in RAM saving measures.

### Operation principle
For info on WSPR protocol, see https://physics.princeton.edu/pulsar/k1jt/wspr.html . 
Succintly, as is custom, every even minute, a WSPR message containing your callsign, locator and tx power is sent during ~163 seconds. For precise timing, uBlox-NEO-6M GPS module receives UTC time. Upon power up, you need clear view of the sky to get a GPS fix, and thus GPS based atomic clock time. Dependent on TX_FRACTION (0 == never transmit, 100 == always transmit), each even minute, a WSPR message will be sent by directly communicating with si5351 chip frequency synthethiser and generating correct frequencies. Before first transmit, you need to calibrate the si5351. The output is not a sinusoid, so everything more than a test needs a band pass filter.

## Hardware
### Minimum
Arduino Nano: Controller

uBlox NEO-6M: GPS module, connect GPS module Vcc to 3v3 pin, its TX pin to GPS_RX_PIN 5, its  RX pin to GPS_TX_PIN 4. 

si5351: frequency synthetiser chip. Connect SDA to A4 and SCL to A5.
### Strongly encouraged
band pass filter: https://pa0fri.home.xs4all.nl/Diversen/LPfilter/Hf%20low%20pass%20filter.htm for a HF low pass filter. Note that harmonics will still exist, i.e. 7MHz will get 14, 21, 28 MHz harmonics. Si5351 is not clean.

Display: sketch is based on 2x16 LC module with I²C adaptor. Feel free to change, as long as you keep in mind ~530 bytes of minimum RAM memory.
### Optional
Battery power: with Arduino, GPS, si5351 and LCD, you'll need a step up converter if you plan on using Li-Ion cells.

Buttons: Reset button wired to RST, if battery powered, and using LCD, you can use momentary switch for LCD LED.

## Software
### Calibration
Needed: Arduino, si5351, either a Radio or a spectrum analyzer (e.g. TinySA).

Before uploading the sketch, be sure to calibrate the si5351 module using the example library sketch included with <si5351.h>. Load up calibration sketch. Tune your radio on 10MHz CW or set up your SA around 10MHz. Open up Serial Monitor. If using radio on CW, you can download Spectroid on Arduino and look at CW tone received (i.e. if your CW tone is set to 700Hz, you should see it peak around 700Hz). Follow Serial monitor instructions until you get 10MHz output (or your set CW tone if using radio + spectroid). Note down the calibration value (tip: write it on your si5351 module). In the sketch, search for si5351.set_correction(), and replace first argument with your calibration value.

### Adapting the sketch to your callsign/locator
If you want to copy the sketch straightforward, all you have to do is connect everything right and adapt everything below "// set up WSPR transmit data" to your own Callsign and Maidenhead locator. "LocatorOption" as far as I know isn't working, leave it, so just enter your 4 character Maidenhead locator below it. Terminate with zero. I.e. replace my "J", "O", "1", "0".

### Adpating band hopping schedule
Your beacon is set up to send a WSPR block every even minute. Bands to send on are stored as unsigned long long in "uband_freq" array. For display purposes, the bands are stored in wavelengths in "band_str" and the frequency in MHz is stored as a string in "band_str1". Default, we have 16 bands programmed.

You can schedule what band/frequency you will send by adapting the array "bandschedule[29]". Each element is an even minute in an hour, and its value is the index of band_str. i.e. 02 minutes = band_str 3, which is 80M band. If you want to make a monoband WSPR transmitter, replace all values in "bandschedule" with the index of your wanted band (I.e. for 40m, replace all 30 values with 5.



