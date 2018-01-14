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
#include "EvdevOutput.hpp"

EvdevOutput::EvdevOutput(const Evdev &e) : flags(0) {
	outdev = libevdev_new();
	libevdev_set_name(outdev, "Screentouch: Touch to mouse translator");
	addType(EV_ABS);
	input_absinfo ia = *e.absInfo(ABS_X);
	addCode(EV_ABS, ABS_X, &ia);
	ia = *e.absInfo(ABS_Y);
	addCode(EV_ABS, ABS_Y, &ia);
	addType(EV_REL);
	addCode(EV_REL, REL_WHEEL);
	addCode(EV_REL, REL_HWHEEL);
	addType(EV_KEY);
	addCode(EV_KEY, BTN_LEFT);
	addCode(EV_KEY, BTN_MIDDLE);
	addCode(EV_KEY, BTN_RIGHT);
	addType(EV_SYN);
	addCode(EV_SYN, SYN_REPORT);
	if (libevdev_uinput_create_from_device(
		outdev,
		LIBEVDEV_UINPUT_OPEN_MANAGED,
		&uoutdev
	)) {
		BOOST_THROW_EXCEPTION(EvdevUInputCreateError());
	}
}

EvdevOutput::~EvdevOutput() {
	libevdev_uinput_destroy(uoutdev);
	libevdev_free(outdev);
}

void EvdevOutput::addType(int t) {
	if (libevdev_enable_event_type(outdev, t)) {
		BOOST_THROW_EXCEPTION(EvdevTypeAddError() <<
			EvdevEventType(t) <<
			EvdevEventTypeName(libevdev_event_type_get_name(t))
		);
	}
}

void EvdevOutput::addCode(int t, int c, const void *p) {
	if (libevdev_enable_event_code(outdev, t, c, p)) {
		BOOST_THROW_EXCEPTION(EvdevCodeAddError() <<
			EvdevEventType(t) <<
			EvdevEventTypeName(libevdev_event_type_get_name(t)) <<
			EvdevEventCode(c) <<
			EvdevEventCodeName(libevdev_event_code_get_name(t, c))
		);
	}
}

void EvdevOutput::set(const EventTypeCode &etc, std::int32_t val) {
	if (libevdev_uinput_write_event(uoutdev, etc.type, etc.code, val)) {
		BOOST_THROW_EXCEPTION(EvdevError() <<
			EvdevEventType(etc.type) <<
			EvdevEventTypeName(etc.typeName()) <<
			EvdevEventCode(etc.code) <<
			EvdevEventCodeName(etc.codeName()) <<
			EvdevEventValue(val)
		);
	}
	// record changes to mouse button states for use in debugging output
	if (etc.type == EV_KEY) {
		int b = etc.code - BTN_LEFT;
		if ((b >= 0) && (b < 3)) {
			flags = (flags & ~(1 << b)) | (val << b);
		}
	}
}

int EvdevOutput::get(const EventTypeCode &etc) const {
	int b = etc.code - BTN_LEFT;
	if ((b >= 0) && (b < 3)) {
		return flags & (1 << b);
	}
	return 0;
}
