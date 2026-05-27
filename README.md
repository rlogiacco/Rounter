# Rounter
Round counter and countdown timer for turn based games like Dungeons & Dragons.

## Hardware

This firmware uses one ESP32 and it's based on the C3 Super Mini variant, but can be adapted to other boards with little to no changes.

The device user interface consists of:

 - two digits seven segment display, each digit using 14 WS2812B LEDs, 2 for each segment, chained togheter
 - three push button: minus, plus and start/settings

## Feeatures

### Counter mode

Right after boot and whenever the screen shows a green digit, the device is in round `COUNTER MODE`, displaying the current round count, starting from 0, the _surprise round_. 

In this mode, the `+` and `-` buttons respectively increment or decrement the current round, up to the max value of `99`. IF both buttons are hold down at the same time, the counter resets to `0`.

A click on the *settings* button switches the device into `TIMER MODE` and starts a countdown timer, either from the last value set or from the default value stored in memory.

Holding the *settings* button switches the device into `SETTINGS MODE` and allows to modify or permanently store the timer duration.

##### IMPORTANT

The current round is never stored in memory and it's lost on power down!

### Timer mode

When in `TIMER MODE` the digits are displayed in either blue or red, the latter is used for the last ten seconds of the timer. The digits displayed on the screen represents either the remaining seconds or the remaining minutes, allowing for a maximum timer duration of 99 minutes (5940 seconds).

If the timer is ticking, the digits will fade off once per second, to signal timer is running. If the timer is paused, the digits will stay lit.

The *settings* button can be used to pause or resume the timer with a single click, while a double click will interrupt the timer and switch back into `COUNTER MODE`.

### Settings mode
