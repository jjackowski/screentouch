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
#include <boost/program_options.hpp>


// used to log intput events for debugging
void logEv(EventTypeCode tc, std::int32_t v) {
	std::cout << "Got event " << tc.typeName() << ':' << tc.codeName() <<
	" with value " << v << std::endl;
}

int main(int argc, char *argv[])
try {
	std::vector<std::string> devpath;
	int movethres;
	bool abs = false;
	{ // option parsing
		boost::program_options::options_description optdesc("");
		optdesc.add_options()
			( // help info
				"help,h",
				"Show this help message"
			)
			( // absolute positioning
				"abs,a",
				"Provide absolute position for the mouse location"
			)
			/* not yet merged
			( // relative positioning
				"rel,r",
				"Use relative motion across the screen for mouse movement (default)"
			)
			*/
			( // movement threshold
				"movethres",
				boost::program_options::value<int>(&movethres)->
					default_value(8),
				"The distance, in pixels, that a contact must move before it is"
				" considered to have moved"
			)
			( // the device file(s) to use
				"dev,d",
				boost::program_options::value< std::vector< std::string > >(&devpath),
				"Specify input device file(s)"
			)
		;
		boost::program_options::positional_options_description posoptdesc;
		// --dev or -d doesn't need to be specified if the file is the last
		// argument given to the program
		posoptdesc.add("dev", -1);
		boost::program_options::variables_map vm;
		boost::program_options::store(
			boost::program_options::command_line_parser(argc, argv).
			options(optdesc).positional(posoptdesc).run(),
			vm
		);
		boost::program_options::notify(vm);
		if (!vm.count("help") && devpath.empty()) {
			std::cerr << "Input device path not provided." << std::endl;
		}
		if (vm.count("help") || devpath.empty()) {
			std::cout << "Screentouch - makes a touchscreen act more like a touchpad.\n" <<
			argv[0] << " [options] [device file(s)]\n" << optdesc << std::endl;
			if (devpath.empty()) {
				return 1;
			}
			return 0;
		}
		if (vm.count("abs")) {
			if (vm.count("rel")) {
				std::cerr << "Cannot provide absolute and relative mouse "
				"positioning simultaneously." << std::endl;
				return 1;
			}
			abs = true;
			std::cout << "Using absolute mouse positioning." << std::endl;
		} else {
			// needs to be merged first
			//std::cout << "Using relative mouse movement." << std::endl;
		}
	}
	// C++ friendly epoll
	Poller poller;
	for (const std::string devarg : devpath) {
		// initialize input
		EvdevShared evin;
		try {
			evin = std::make_shared<Evdev>(devarg);
		} catch (EvdevError &) {
			std::cerr << "Failed to open " << devarg << '.' << std::endl;
			continue;
		}
		// check for touch input
		if (!evin->hasEventType(EV_ABS) || (evin->numSlots() < 0)) {
			std::cerr << "Device " << devarg << ", " << evin->name() <<
			", is not a touch screen." << std::endl;
			continue;
		}
		std::cout << "Using device " << devarg << ", " << evin->name() << '.'
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
		MtTranslate ms(evin, movethres);
		do {
			if (!poller.wait(std::chrono::milliseconds(192))) {
				ms.timeoutHandle();
			}
		} while (true);
		return 0;
	}
	std::cerr << "No touchscreen found." << std::endl;
	return 1;
} catch (EvdevUInputCreateError &) {
	std::cerr << "Failed to create the user input device. /dev/uinput may not exist,"
	" or may not be readable and writeable from this user account." << std::endl;
	return 2;
} catch (...) {
	std::cerr << "Program failed:\n" <<
	boost::current_exception_diagnostic_information()
	<< std::endl;
	return 3;
}