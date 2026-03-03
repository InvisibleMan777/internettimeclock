# INTERNET-TIME CLOCK

## Description
This project has been made in asignment of Windesheim university of applied sciences. The goal is to create a system that shows the current time in [swatch's internet-time](https://www.swatch.com/nl-nl/internet-time.html), and synchs with the internet via (S)NTP. 

### The system encompasses:
- An OLED LCD that shows the time and networkstatus
- A stepper motor that functions as a hand on an analog clock, it makes a full rotaton every 100 centibeats (1 beat)
- A rotery encoder that can be used to change the position of the stepper motor
- A buzzer that makes a tick sound every centibeat
- A low power mode that turns of the LCD to save power
- 3 Leds that indicate: timeupdates, the networkstatus, and the mode of the stepper motor

## getting started
(The system needs specifick hardware, found in `docs/technish_ontwerp.pdf`)

Install the hardware

Download the sourcecode and import it as a platformio project

Change the cofiguration in `src/main.c` (wifi ssid, password, gpio, i2c address for the oled display, ect...). <br>
It is also possible to save configuration and call from a .env file. The `create_env.py` script makes sure they get added to the build.

Build and run

consult `docs/functioneel_ontwerp` for usage clarification

<b>problems? </b> <br>
contact the developer

## Contributer
<b>Sem Overbeek</b> is the only contributer

### contact:
- <b>email:</b> s1212918@student.windesheim.nl