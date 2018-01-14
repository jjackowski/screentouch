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
#include <boost/exception/errinfo_file_name.hpp>
#include <boost/exception/errinfo_errno.hpp>

// for open() and related items; may be more than needed
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

const char *EventTypeCode::typeName() const {
	return libevdev_event_type_get_name(type);
}

const char *EventTypeCode::codeName() const {
	return libevdev_event_code_get_name(type, code);
}

//Evdev::Evdev() : dev(nullptr), fd(-1) { }

Evdev::Evdev(const std::string &path) {
	fd = open(path.c_str(), O_RDONLY);
	if (fd < 0) {
		BOOST_THROW_EXCEPTION(EvdevFileOpenError() <<
			boost::errinfo_file_name(path)
		);
	}
	int result = libevdev_new_from_fd(fd, &dev);
	if (result < 0) {
		close(fd);
		BOOST_THROW_EXCEPTION(EvdevInitError() <<
			boost::errinfo_errno(-result) <<
			// the file may have nothing to do with the error, but it will
			// add context
			boost::errinfo_file_name(path)
		);
	}
}

Evdev::Evdev(Evdev &&e) : dev(e.dev), fd(e.fd) {
	e.dev = nullptr;
	e.fd = -1;
}

Evdev::~Evdev() {
	if (dev) {
		libevdev_free(dev);
	}
	if (fd > 0) {
		close(fd);
	}
}

Evdev &Evdev::operator=(Evdev &&old) {
	dev = old.dev;
	old.dev = nullptr;
	fd = old.fd;
	old.fd = -1;
}

void Evdev::respond(int) {
	input_event ie;
	int result;
	do {
		result = libevdev_next_event(
			dev,
			LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING,
			&ie
		);
		if (result == LIBEVDEV_READ_STATUS_SUCCESS) {
			EventTypeCode etc(ie.type, ie.code);
			InputMap::const_iterator iter = receivers.find(etc);
			if (iter != receivers.end()) {
				iter->second(etc, ie.value);
			}
		}
	} while ((result >= 0) && (libevdev_has_event_pending(dev) > 0));
}

std::string Evdev::name() const {
	return libevdev_get_name(dev);
}

bool Evdev::grab() {
	return libevdev_grab(dev, LIBEVDEV_GRAB) == 0;
}

bool Evdev::hasEventType(unsigned int et) const {
	return libevdev_has_event_type(dev, et) == 1;
}

bool Evdev::hasEventCode(unsigned int et, unsigned int ec) const {
	return libevdev_has_event_code(dev, et, ec) == 1;
}

bool Evdev::hasEvent(EventTypeCode etc) const {
	return libevdev_has_event_code(dev, etc.type, etc.code) == 1;
}

int Evdev::numSlots() const {
	return libevdev_get_num_slots(dev);
}

int Evdev::value(unsigned int et, unsigned int ec) const {
	int val;
	if (!libevdev_fetch_event_value(dev, et, ec, &val)) {
		BOOST_THROW_EXCEPTION(EvdevUnsupportedEvent() <<
			EvdevEventType(et) << EvdevEventCode(ec)
		);
	}
	return val;
}

void Evdev::usePoller(Poller &p) {
	p.add(shared_from_this(), fd);
}

const input_absinfo *Evdev::absInfo(unsigned int absEc) const {
	const input_absinfo *ia = libevdev_get_abs_info(dev, absEc);
	if (!ia) {
		BOOST_THROW_EXCEPTION(EvdevUnsupportedEvent() <<
			EvdevEventType(EV_ABS) << EvdevEventCode(absEc)
		);
	}
	return ia;
}
