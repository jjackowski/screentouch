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

void MtTranslate::init() {
	cur = slot = scnt = cntctCur = cntctOld = cursorX = cursorY = 0;
	old = 1;
	curOp = None;
	eventtime = std::chrono::steady_clock::now();
	// configure reception of mulit-touch input events
	evdev->inputConnect(
		EventTypeCode(EV_ABS, ABS_MT_SLOT),
		// MUST use boost::bind, not std::bind, here
		boost::bind(&MtTranslate::slotEvent, this, _2)
	);
	evdev->inputConnect(
		EventTypeCode(EV_ABS, ABS_MT_TRACKING_ID),
		boost::bind(&MtTranslate::trackEvent, this, _2)
	);
	evdev->inputConnect(
		EventTypeCode(EV_ABS, ABS_MT_POSITION_X),
		boost::bind(&MtTranslate::xPosEvent, this, _2)
	);
	evdev->inputConnect(
		EventTypeCode(EV_ABS, ABS_MT_POSITION_Y),
		boost::bind(&MtTranslate::yPosEvent, this, _2)
	);
	evdev->inputConnect(
		EventTypeCode(EV_SYN, SYN_REPORT),
		boost::bind(&MtTranslate::synEvent, this)
	);
}

MtTranslate::MtTranslate(const EvdevShared &ev) :
evdev(ev), eo(*evdev), slots(evdev->numSlots()) {
	init();
}

MtTranslate::MtTranslate(EvdevShared &&ev) :
evdev(std::move(ev)), eo(*evdev), slots(evdev->numSlots()) {
	init();
}

MtTranslate::~MtTranslate() {
	// should disconnect from evdev; not yet supported
}

void MtTranslate::slotEvent(std::int32_t val) {
	slot = val;
}

void MtTranslate::trackEvent(std::int32_t val) {
	slots[slot][cur].tid = val;
	if (val < 0) {
		--scnt;
	} else {
		++scnt;
	}
	//std::cout << "trackEvent: id = " << val << "  slots = " << scnt << std::endl;
	assert((scnt >= 0) && (scnt <= slots.size()));
}

void MtTranslate::xPosEvent(std::int32_t val) {
	slots[slot][cur].x = val;
}

void MtTranslate::yPosEvent(std::int32_t val) {
	slots[slot][cur].y = val;
}

void MtTranslate::synEvent() {
	timepoint currtime = std::chrono::steady_clock::now();
	bool updateCursor = false;

	cntctCur = scnt;
	// start contact
	if (!cntctOld && cntctCur) {
		// previous contact not long ago?
		if (curOp && (curOp <= ReleaseMiddle)) {
			duration span = currtime - eventtime;
			if (span <= tapTime) {
				// transition to drag operation & press button
				switch (curOp) {
					case ReleaseLeft:
						curOp = DragLeft;
						eo.set(EventTypeCode(EV_KEY, BTN_LEFT), 1);
						break;
					case ReleaseRight:
						curOp = DragRight;
						eo.set(EventTypeCode(EV_KEY, BTN_RIGHT), 1);
						break;
					case ReleaseMiddle:
						curOp = DragMiddle;
						eo.set(EventTypeCode(EV_KEY, BTN_MIDDLE), 1);
						break;
				}
				updateCursor = true;
			}
		} else {
			// do not respond to a release condition; time has expired
			curOp = None;
			eventtime = currtime;
			// should always be the first slot
			assert(slots[0][cur].tid >= 0);
			// store contact position as cursor, but do not update cursor
			cursorX = slots[0][cur].x;
			cursorY = slots[0][cur].y;
		}
	}
	// end contact
	else if (!scnt) {
		// move the cursor if not scrolling
		if (curOp < ScrollVert) {
			updateCursor = true;
		}
		switch (curOp) {
			case None:
				// change operation to release
				curOp = ReleaseLeft - 1 + cntctOld;
				eventtime = currtime;
				break;
			case DragLeft:
				curOp = None;
				eo.set(EventTypeCode(EV_KEY, BTN_LEFT), 0);
				break;
			case DragRight:
				curOp = None;
				eo.set(EventTypeCode(EV_KEY, BTN_RIGHT), 0);
				break;
			case DragMiddle:
				curOp = None;
				eo.set(EventTypeCode(EV_KEY, BTN_MIDDLE), 0);
				break;
		}
		cntctOld = 0;
	}
	// fewer contact(s)
	else if (cntctOld > cntctCur) {
		// fingers may come off one at a time; keep max contacts to make it
		// easy to use right & middle buttons (2 & 3 contacts respectively)
		cntctCur = cntctOld;
	}

	// cursor movement
	if (scnt && ((cntctCur == cntctOld) || !updateCursor)) {
		bool sync = false;
		// start cursor motion?
		if (curOp == None) {
			// look for a change
			int deltaX = std::abs(slots[0][cur].x - cursorX);
			int deltaY = std::abs(slots[0][cur].y - cursorY);
			if ((deltaX > moveDist) || (deltaY > moveDist)) {
				// request to move cursor?
				if ((curOp == None) && (cntctCur == 1)) {
					curOp = MoveCursor;
				}
				// scroll 1D
				else if (cntctCur == 2) {
					// scroll vertically?
					if (deltaY > deltaX) {
						curOp = ScrollVert;
					}
					// scroll horizontally?
					else if (deltaY < deltaX) {
						curOp = ScrollHoriz;
					}
				}
				// scroll 2D
				else if (cntctCur == 3) {
					curOp = Scroll2D;
				}
			}
		}
		// current operation involves moving the cursor
		if (
			// operation requires moving the cursor
			(curOp >= DragLeft) && (curOp <= MoveCursor) &&
			// position has changed
			((cursorX != slots[0][cur].x) || (cursorY != slots[0][cur].y))
		) {
			updateCursor = true;
		}
		// vertical scroll operation
		else if ((curOp == ScrollVert) || (curOp == Scroll2D)) {
			// look for a change
			int delta = (slots[0][cur].y - cursorY) >> 3;
			if (delta) {
				cursorY = slots[0][cur].y;
				eo.set(EventTypeCode(EV_REL, REL_WHEEL), delta);
				sync = true;
			}
		}
		// horizontal scroll operation
		else if ((curOp == ScrollHoriz) || (curOp == Scroll2D)) {
			// look for a change
			int delta = (cursorX - slots[0][cur].x) >> 3;
			if (delta) {
				cursorX = slots[0][cur].x;
				eo.set(EventTypeCode(EV_REL, REL_HWHEEL), delta);
				sync = true;
			}
		}
		// sync for scroll events
		if (sync) {
			eo.sync();
		}
	}

	if (updateCursor) {
		// always using slot 0 is easy, but will cause cursor to suddenly move
		// on mulitple finger double-tap if fingers contact in different order
		// the second time
		cursorX = slots[0][cur].x;
		cursorY = slots[0][cur].y;
		eo.set(EventTypeCode(EV_ABS, ABS_X), cursorX);
		eo.set(EventTypeCode(EV_ABS, ABS_Y), cursorY);
		eo.sync();
	}

	// advance current to old
	cntctOld = cntctCur;
	//logstate();
}

void MtTranslate::timeoutHandle() {
	// check for waiting on user to touch again
	if (curOp && (curOp <= ReleaseMiddle)) {
		// time up?
		timepoint currtime = std::chrono::steady_clock::now();
		duration span = currtime - eventtime;
		if (span >= tapTime) {
			// re-send position in case another input device moved the cursor
			eo.set(EventTypeCode(EV_ABS, ABS_X), cursorX);
			eo.set(EventTypeCode(EV_ABS, ABS_Y), cursorY);
			// press button
			int button;
			switch (curOp) {
				case ReleaseLeft:
					button = BTN_LEFT;
					break;
				case ReleaseRight:
					button = BTN_RIGHT;
					break;
				case ReleaseMiddle:
					button = BTN_MIDDLE;
					break;
			}
			eo.set(EventTypeCode(EV_KEY, button), 1);
			eo.sync();
			// release button
			eo.set(EventTypeCode(EV_KEY, button), 0);
			eo.sync();
			// done with this operation
			curOp = None;
			
			//std::cout << "Released" << std::endl;
			//logstate();
		}
	}
}

void MtTranslate::logstate() const {
	static const char *opstr[Scroll2D+1] = {
		"None",
		"RelLeft",
		"RelRight",
		"RelMiddle",
		"DragLeft",
		"DragRight",
		"DragMiddle",
		"MoveCursor",
		"ScrollVert",
		"ScrollHoriz",
		"Scroll2D"
	};
	// prevously kept writing over the same line
	std::cout /* << '\r' */ << std::setw(12) << opstr[curOp] << ' ' <<
	std::setw(3) << cursorX << ", " << std::setw(3) << cursorY << "  " <<
	std::setw(2) << cntctCur << "  ";// << std::endl; //"   ";
	if (eo.get(EventTypeCode(EV_KEY, BTN_LEFT))) {
		std::cout << 'L';
	} else {
		std::cout << ' ';
	}
	if (eo.get(EventTypeCode(EV_KEY, BTN_MIDDLE))) {
		std::cout << 'M';
	} else {
		std::cout << ' ';
	}
	if (eo.get(EventTypeCode(EV_KEY, BTN_RIGHT))) {
		std::cout << 'R';
	} else {
		std::cout << ' ';
	}
	std::cout << "   " << std::endl;
	//std::cout.flush();
}
