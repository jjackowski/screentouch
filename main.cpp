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
#include "MtTranslate.hpp"
#include <iostream>
#include <fstream>
#include <boost/exception/diagnostic_information.hpp>

// used to log intput events for debugging
void logEv(EventTypeCode tc, std::int32_t v) {
	std::cout << "Got event " << tc.typeName() << ':' << tc.codeName() <<
	" with value " << v << std::endl;
}

int main(int argc, char *argv[])
try {
	// C++ friendly epoll
	Poller poller;
	for (int c = 1; c < argc; ++c) {
		// initialize input
		EvdevShared evin;
		try {
			evin = std::make_shared<Evdev>(argv[c]);
		} catch (EvdevError &) {
			std::cerr << "Failed to open " << argv[c] << '.' << std::endl;
			continue;
		}
		// check for touch input
		if (!evin->hasEventType(EV_ABS) || (evin->numSlots() < 0)) {
			std::cerr << "Device " << argv[c] << ", " << evin->name() <<
			", is not a touch screen." << std::endl;
			continue;
		}
		std::cout << "Using device " << argv[c] << ", " << evin->name() << '.'
		<< std::endl;
		if (!evin->grab()) {
			std::cerr << "Cannot gain exclusive access." << std::endl;
		}
		/*  for logging input events from the touch screen
		evin->inputConnect(EventTypeCode(EV_ABS, ABS_MT_SLOT), &logEv);
		evin->inputConnect(EventTypeCode(EV_ABS, ABS_MT_TRACKING_ID), &logEv);
		evin->inputConnect(EventTypeCode(EV_ABS, ABS_MT_POSITION_X), &logEv);
		evin->inputConnect(EventTypeCode(EV_ABS, ABS_MT_POSITION_Y), &logEv);
		evin->inputConnect(EventTypeCode(EV_SYN, SYN_REPORT), &logEv);
		*/
		evin->usePoller(poller);
		MtTranslate ms(evin);
		do {
			if (!poller.wait(std::chrono::milliseconds(192))) {
				ms.timeoutHandle();
			}
		} while (true);
		return 0;
	}
	std::cerr << "No touchscreen found." << std::endl;
	return 1;
} catch (...) {
	std::cerr << "Program failed:\n" <<
	boost::current_exception_diagnostic_information()
	<< std::endl;
}