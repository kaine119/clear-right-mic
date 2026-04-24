# Lecture mic, Supercharged by AI (tm) (r) (c) (bbq)

> Done up as part of 30.201 Wireless Communications & IoT

Firmware for the mic part of a teaching feedback system to let professors know
when their words might be going over their students' heads.

This half is an ESP32 MCU with an INMP441 microphone. It records some voice
audio, sends it up to the Gemini API to make an inference on whether it would
make sense to a first-year engineering college student, and updates an ESP
Rainmaker attribute accordingly.

## Demo

[Video](assets/vid.mp4)

## Hardware setup

BOM:

* 1x ESP32-C6-DevKitC-1
* 1x INMP441 microphone (available as a breakout board on Aliexpress)

The mic is wired up as a typical I2S device:

| INMP441 board | ESP32 |
| ------------- | ----- |
| SCK           | 3     |
| WS            | 2     |
| SD            | 10    |
| L/R           | GND   |
| VDD           | 3.3V  |
| GND           | GND   |

Note that, since we only need one mic and we only use the left channel, we
ground L/R to get the INMP441 to only output on the left I2S slot.

## Software setup

* Put in your Gemini API key in
  `components/inference_api/include/gemini_config.h`. DO NOT commit this file!
  Otherwise hax0rs will steal your API key and charge one million dollah on your
  card!
* Build and flash as usual, and use the ESP Rainmaker app to provision the
  device onto a network.

## Data flow

* [`mic_task`](components/mic/mic.c) periodically runs, records some audio data
  from the I2S bus and saves it as a `.wav` file to a SPIFFS partition
  (in chunks).

  `mic_task` sends a message to `recording_queue` to signal that a new recording
  is available.

* [`api_task`](components/inference_api/inference_api.c) receives the filename
  and size of a new recording, and uploads the recording to [Google Gemini](https://ai.google.dev/gemini-api/docs/audio),
  using the [Files API](https://ai.google.dev/gemini-api/docs/files). It then makes a `generateContent` model request, using the system prompt: 

  > You are a first-year university engineering student. Transcribe the
    following recordings, and tell me if they make sense to you from a
    student's perspective. Format your answer as follows: YES/NO // Transcript:
    \<transcript\> // Thoughts: \<further context\>.

  When a response is received, it is parsed to a boolean according to the first
  few characters of the model output, and a message is sent to
  `api_response_queue` to signal that a new inference is available.

* [`status_updater`](components/status_updater/status_updater.c) receives a bool
  based on the LLM output, and updates an ESP-Rainmaker parameter, indicating
  whether or not the recorded message correlates to something that a student
  might understand.


