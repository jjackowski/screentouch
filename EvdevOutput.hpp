/*
 * This file is part of the Screentouch project. It is subject to the GPLv3
 * license terms in the LICENSE file found in the top-level directory of this
 * distribution and at
 * https://github.com/jjackowski/screentouch/blob/master/LICENSE.
 * No part of the Screentouch project, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE file.
 *
 * Copyright (C) 2018  Jeff Jackowski
 */
#include "Evdev.hpp"
#include <libevdev/libevdev-uinput.h>

/**
 * Outputs input events to a user-space input (uinput) device using libevdev.
 * Much of this class is very specific to the screentouch project, but it
 * could be refactored to be more generic.
 * @author  Jeff Jackowski
 */
class EvdevOutput {
	/**
	 * The input device that this object will create.
	 */
	libevdev *outdev;
	/**
	 * The device to which input events will be output.
	 */
	libevdev_uinput *uoutdev;
	/**
	 * Used to track mouse button states for debugging.
	 */
	int flags;
	/**
	 * Adds an event type to the input device.
	 * @param t  The event type, such as EV_KEY.
	 * @throw    EvdevTypeAddError  libevdev reported an error with the
	 *                              add attempt.
	 */
	void addType(int t);
	/**
	 * Adds an event code to the input device.
	 * @param t  The event type, such as EV_KEY.
	 * @param c  The event code, such as BTN_LEFT.
	 * @param p  A pointer to a input_absinfo struct (defined in Linux kernel
	 *           code) if the event type is EV_ABS, otherwise it must be nullptr.
	 *           The data in the input_absinfo struct will be copied, so it need
	 *           not be maintained.
	 * @throw    EvdevCodeAddError  libevdev reported an error with the
	 *                              add attempt.
	 */
	void addCode(int t, int c, const void *p = nullptr);
public:
	/**
	 * Makes a new input device to output input events specifically for the
	 * screentouch project.
	 * @param e  The touchscreen input device. Needed to query for input_absinfo
	 *           data on the axes.
	 */
	EvdevOutput(const Evdev &e);
	/**
	 * Destroys created input devices.
	 */
	~EvdevOutput();
	/**
	 * Sends an input event.
	 */
	void set(const EventTypeCode &etc, std::int32_t val);
	/**
	 * Queries mouse button states for debugging.
	 */
	int get(const EventTypeCode &etc) const;
	/**
	 * Sends a SYN_REPORT event to signal to input users that input should now
	 * be processed. Input events between SYN_REPORT events are considered to
	 * have occurred simultaneously. As a result, input events will seem to be
	 * ignored until a SYN_REPORT event is sent.
	 */
	void sync() {
		set(EventTypeCode(EV_SYN, SYN_REPORT), 0);
	}
};
