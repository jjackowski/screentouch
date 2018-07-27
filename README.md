# Screentouch
An attempt at making touchscreens more useful for desktop-style programs that are written for use with a mouse (and hopefully a keyboard).

I didn't like that putting a finger on my Raspberry Pi's touchscreen always acted as pressing the left mouse button, and no other buttons were available. I wrote this program to translate the touchscreen's input into something more like touchpads commonly found on notebook computers. While I did all the testing on a Raspberry Pi with the foundation's touchscreen, the program isn't specific to that hardware. It does, however, need a touchscreen with stateful contact reporting. The Linux kernel documentation calls this "multitouch protocol B". Some screens, like the Raspberry Pi's, may see only a single contact if multiple fingers are used but kept close together.

The program needs to get input device files as arguments when invoked. It will inspect each of these files until it finds one that looks like a touchscreen and will then use that one input device. Next, it creates a new input device using uinput, Linux's user-space input device support. It attempts to gain exclusive access to the touchscreen input to prevent software from responding to the touchscreen directly since the software may also respond to the new uinput device as well. Then it translates the touchscreen input into what looks more like a mouse.

# Input translation

|Action                 | One finger       | Two fingers       | Three fingers
|-----------------------|------------------|-------------------|-----------------
|Press & move           | Move cursor      | 1D scroll         | 2D scroll
|Press & release        | Left button      | Right button      | Middle button
|Press, release & press | Drag left button | Drag right button | Drag middle button

One dimensional scrolling selects either the horizontal or vertical axis based on the motion of the fingers. Two dimensional scrolling will scroll both ways, but doesn't seem to work with Firefox. Scrolling does not move the mouse cursor.

Double click type action isn't working well at the moment.

# Raspbian

I haven't tried to build the program on Raspbian, but that should work. However, I have built it on Gentoo, copied it to a Raspbian install, and it ran just fine. See the running section below.

# Building

Prerequisites:
- gcc with C++11 support
- Boost Exception and Signals2 libraries, version 1.50 and up
- libevdev, version 1.57 works but older may be fine
- Scons, version 2.x

Run scons from the directory with the SConstruct file. If successful, the results will be under a directory for the host's platform under the bin directory.

The build has several options that can be changed if the defaults cause trouble. They are shown by running "scons -h". Change them by adding OPTIONNAME=value as an argument to scons.

# Running

The Linux kernel's user-space input device support must be available. Many kernels are built with the support in a module called uinput, including Raspbian's. It makes a device file that is usually /dev/uinput, but may be /dev/input/uinput. The kernel module probably won't be loaded by default, but it must be loaded or built into the kernel (no module) for Screentouch to work.

The user account used to run Screentouch must have read and write access to the uinput file, and to the touchscreen input device file. On some Linux distributions, users that are in the input group will have read and write access to the /dev/input/event* input device files, but may not have similar access to the uinput device file.

The input device files may not always have the same numbers for the same devices. On my Raspberry Pi, it will be /dev/input/event0 if no other input devices were available on boot. Otherwise it will be numbered after other input devices, like a USB keyboard. This is why the program will inspect multiple device files to find a touchscreen.

Run the Screentouch program with the input device file(s) to try as arguments. On a Raspberry Pi from the directory with the SConstruct file, it may look like this:

bin/linux-armv7l-dbg/screentouch /dev/input/event*

The program can be run after an X server is running and get the desired result. If the program terminates while an X server is running, the server will again see input from the touchscreen device. An X server is not required; mouse-like input will be made available the entire time Screentouch is running, but the absolute coordinates will always be in terms of the screen's pixels.

# Udev

Most Linux distributions these days run udev to set device file permissions as the files show up. On my system, I made /dev/uinput readable and writable by users in the input group by adding the file /etc/udev/rules.d/uinput.rule with this line:

KERNEL=="uinput", GROUP="input", MODE="0660"

This will allow users in the input group to create input devices and subsequently send input. This means any user in the input group could send input to the console, which could affect other users. It is probably better to either create another group, or limit this to the single user account that will run Screentouch.