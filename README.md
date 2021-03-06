# Project Horus's Low-Speed Binary Telemetry System
This repository contains documentation and scripts to work with the new `horus_demod` MFSK/RTTY demodulator, developed by [David Rowe](http://rowetel.com). Currently this demodulator provides ~2.5dB better RTTY decode performance than dl-fldigi 3.21.50, and ~0.5dB better performance than fldigi 4.0.1.

More importantly, it also adds support for a binary-packet 4FSK mode, designed specifically for high-altitude balloon telemetry, and which is intended to supercede RTTY for all Project Horus launches. Preliminary testing shows it has ~6dB improved demodulation performance over RTTY at the same baud rate.

Currently we are developing the modem under Linux & OSX, with the eventual aim to produce a cross-platform GUI. For now, the demodulator is available as a command-line utility, with additional binary packet processing and uploading of data to Habitat performed by the `horusbinary.py` python script.

These modems have recently been added to the FreeDV GUI, to allow easier usage. Refer to this guide for instructions on using FreeDV to decode Horus Binary telemetry: https://github.com/projecthorus/horusbinary/wiki/FreeDV---HorusBinary-Setup-&-Usage-Instructions

## Modes Supported
The `horus_demod` modem (located within the codec2-dev repo) is in early development, and currently only supports:

### MFSK - Horus Binary Packets
Horus Binary packets take the form:
```
<preamble><unique word><payload>
where
<preamble> = 0x1B1B
<unique word> = 0x1B1B2424
```
The payload consists of a 22-byte long binary packet, encoded with a Golay (23,12) code, and then interleaved and scrambled, for a total encoded length of 43 bytes. The binary packet format is [available here](https://github.com/darksidelemm/RS41HUP/blob/master/main.c#L72), and the golay-encoding/interleaving/scrambling is performed by [horus_l2_encode_packet](https://github.com/darksidelemm/RS41HUP/blob/master/horus_l2.c#L117).

At the start of a packet is a Payload ID (one byte). A lookup table for payload IDs is [located here](https://github.com/projecthorus/horusbinary/blob/master/payload_id_list.txt). **If you are going to fly your own payload using this mode, you must get a payload ID allocated for your use. This can be done by submitting an issue or a pull request to this repository, or e-mailing me at vk5qi (at) rfhead.net**

Packets are then transmitted using **4FSK modulation**, at **100 baud**.

A worked example for generating and encoding these packets is available in the [RS41HUP](https://github.com/darksidelemm/RS41HUP/blob/master/main.c#L401) repository.

### LDPC Horus Binary Packets
Reduced Packets of 14 bytes are possible, for testing.

## Hardware Requirements
The MFSK modes are narrow bandwidth, and can be received using a regular single-sideband (SSB) radio receiver. This could be a 'traditional' receiver (like a Icom IC-7000, Yaesu FT-817 to name but a few), or a software-defined radio receiver. The point is we need to receive the on-air signal (we usually transmit on 70cm) with an Upper-Sideband (USB) demodulator, and then get that audio into your computer.

If you are using a traditional receiver, you'll likely either have some kind of audio interface for it, or will be able to connect an audio cable between it and your computer's sound card. Easy!

With a RTLSDR, you will need to use software like [GQRX](http://gqrx.dk/) (Linux/OSX), [SDR#](https://airspy.com/download/), or [SDR Console](http://www.sdr-radio.com/) to perform the USB demodulation. You'll then need some kind of loop-back audio interface to present that audio as a virtual sound card. This can be done using:
* Linux - via the snd-aloop module. Some information on this is [here](https://blog.getreu.net/_downloads/snd-aloop-device.pdf).
* OSX - Using the [SoundFlower](https://github.com/mattingalls/Soundflower) application.
* Windows - Use [VBCable](http://vb-audio.pagesperso-orange.fr/Cable/index.htm)

You're also going to need some sort of antenna to receive the signal from the balloon payload, but I figure that's a bit out of scope for this readme!


## Downloading this Repository
You can either clone this repository using git:
```
$ git clone https://github.com/ed6e0f17/horusbinary.git
```
or download a zip file of the repository [from here](https://github.com/ed6e0f17/horusbinary/archive/master.zip).


## Building Horus-Demod

### Build Dependencies
We may require a few dependencies to be able to use the new modem. Under Ubuntu/Debian, you can install the required packages using:
```
$ sudo apt-get install cmake build-essential libfftw3-dev libcurl4-openssl-dev libncurses5-dev

```

### Compiling gateway binary
We need to compile the gateway binary (based on DaveAke`s LoRa Gateway). This can be accomplished by performing (within this directory):
```
$ cd src
$ make
$ cp gateway ../
$ cd ../
```

## Configuration File
Copy the example configuration file, i.e.:
```
$ cp src/gateway.txt ./
```

The file `gateway.txt` should then be modified with your info before use, and to select the mode ( 0:normal, 1:ldpc, 2:rtty100 3:rtty300 )


## Gateway Usage
The `gateway` binary accepts 48khz 16-bit signed-integer samples via stdin, and can decode MFSK packets at 100 baud.

Suitable audio inputs could be from a sound card input, or from a SDR receiver application such as GQRX or rtl_fm).

We can string these applications together in the command shell using 'pipes', as follows:

### Demodulating from a Sound Card
```
$ sox -d -r 48k -c 1 -t s16 - | ./gateway
```
The above command records from the default sound device.

### Demodulating using rtl_fm
This assumes you want to use an rtl-sdr dongle on a headless Linux machine.
```
rtl_fm -M raw -s 48000 -p 0 -f 434649000 | ./gateway -q
```
Tune 1000 Hz below the expected lower frequency (i.e. 434.650 MHz - 1000 Hz = 434.649 MHz = 434649000 Hz, as above), and make sure that your dongle has a known ppm adjustment.

Can also decode 300 baud RTTY telemetry by changing the mode in the "gateway.txt" config file ( TODO: command line option to change mode.)

### Demodulating using GQRX 
This assumes you have GQRX installed (`sudo apt-get install gqrx` or similar) and working, have set up a USB demodulator over the signal of interest, and have enabled the [UDP output option](http://gqrx.dk/doc/streaming-audio-over-udp) by clicking the UDP button at the bottom-right of the GQRX window.

```
$ nc -l -u localhost 7355 | ./gateway
```
On some platforms nc requires the listen port to be specified with the -p argument. In those cases, use:
```
$ nc -l -u -p 7355 localhost | ./gateway
```
