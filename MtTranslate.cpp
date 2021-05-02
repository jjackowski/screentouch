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
	cur = slot = scnt = cntctCur = cntctOld = relativeX = relativeY = 0;
	old = 1;
	curOp = None;
	eventtime = std::chrono::steady_clock::now();
	// configure reception of mulit-touch input events
	evdev->inputConnect(
		EventTypeCode(EV_ABS, ABS_MT_SLOT),
		std::bind(&MtTranslate::slotEvent, this, std::placeholders::_2)
	);
	evdev->inputConnect(
		EventTypeCode(EV_ABS, ABS_MT_TRACKING_ID),
		std::bind(&MtTranslate::trackEvent, this, std::placeholders::_2)
	);
	evdev->inputConnect(
		EventTypeCode(EV_ABS, ABS_MT_POSITION_X),
		std::bind(&MtTranslate::xPosEvent, this, std::placeholders::_2)
	);
	evdev->inputConnect(
		EventTypeCode(EV_ABS, ABS_MT_POSITION_Y),
		std::bind(&MtTranslate::yPosEvent, this, std::placeholders::_2)
	);
	evdev->inputConnect(
		EventTypeCode(EV_SYN, SYN_REPORT),
		std::bind(&MtTranslate::synEvent, this)
	);
	evdev->inputConnect(
		EventTypeCode(EV_KEY, KEY_LEFTMETA),
		std::bind(&MtTranslate::buttonEvent, this, std::placeholders::_2)
	);
	// get digitizer resolution
	int paramsX[6] = {};
	int paramsY[6] = {};
	ioctl(evdev->fd, EVIOCGABS(ABS_MT_POSITION_X), paramsX);
	ioctl(evdev->fd, EVIOCGABS(ABS_MT_POSITION_Y), paramsY);
	if (paramsX[2]) {
		maxX = paramsX[2];
		cursorX = maxX / 2;
	}
	if (paramsY[2]) {
		maxY = paramsY[2];
		cursorY = maxY / 2;
	}
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

void MtTranslate::buttonEvent(std::int32_t val) {
	buttonHome = val;
	/*
	* if (buttonHome) {
	* 	// key pressed
	* 	...
	* } else {
	* 	// key released
	* 	...
	* }
	*/
}

void MtTranslate::synEvent() {
	timepoint currtime = std::chrono::steady_clock::now();
	bool updateCursor = false;
	
	cntctCur = scnt;
	// start contact
	if (!cntctOld && cntctCur) {
		contacttime = currtime;
		tapX = slots[0][cur].x;
		tapY = slots[0][cur].y;
		relativeX = slots[0][cur].x;
		relativeY = slots[0][cur].y;
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
		}
	}
	// end contact
	else if (!scnt) {
		// move the cursor if not scrolling
		if ((curOp > None) && (curOp < ScrollVert)) {
			updateCursor = true;
		}
		switch (curOp) {
			case MoveCursor:
				tapRightClick = false;
				break;
			case None:
				if (!tapRightClick) {
					// change operation to release
					curOp = ReleaseLeft - 1 + cntctOld;
				} else {
					tapRightClick = false;
				}
				eventtime = currtime;
				break;
			case DragLeft:
				curOp = None;
				eo.set(EventTypeCode(EV_KEY, BTN_LEFT), 0);
				
				// request dragging with no movement, so it looks like double click
				if (!DragLeftBegin) {
					curOp = DoubleClick;
				}
				DragLeftBegin = false;
				
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
			
			// initial design of tap and hold for right button click function
			// it simple uses noicy skipped cursor movements, no timeout handling
			duration span = currtime - contacttime;
			if ((span > tapRightClickDuration) && !tapRightClick) {
				eo.set(EventTypeCode(EV_KEY, BTN_RIGHT), 1);
				eo.sync();
				eo.set(EventTypeCode(EV_KEY, BTN_RIGHT), 0);
				eo.sync();
				tapRightClick = true;
			}
			
			// look for a change
			int deltaX = std::abs(slots[0][cur].x - relativeX);
			int deltaY = std::abs(slots[0][cur].y - relativeY);
			if ((deltaX > moveDist) || (deltaY > moveDist)) {
				// update virtual coordinates to avoid cursor jumping moveDist
				relativeX = slots[0][cur].x + 1;
				relativeY = slots[0][cur].y + 1;
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
			((relativeX != slots[0][cur].x) || (relativeY != slots[0][cur].y))
		) {
			// skip noisy cursor movement on start dragging so that 
			// the system can recognize the double clicks
			if ((curOp == DragLeft) && !DragLeftBegin) {
				relativeX = slots[0][cur].x;
				relativeY = slots[0][cur].y;
				int deltaX = std::abs(slots[0][cur].x - tapX);
				int deltaY = std::abs(slots[0][cur].y - tapY);
				// skip dragging until the minimal move distance is reached
				if ((deltaX > moveDist) || (deltaY > moveDist)) {
					DragLeftBegin = true;
				}
			}
			updateCursor = true;
		}
		// vertical scroll operation
		else if ((curOp == ScrollVert) || (curOp == Scroll2D)) {
			// look for a change
			int delta = (slots[0][cur].y - relativeY) >> 3;
			if (delta) {
				relativeY = slots[0][cur].y;
				eo.set(EventTypeCode(EV_REL, REL_WHEEL), delta);
				sync = true;
			}
		}
		// horizontal scroll operation
		else if ((curOp == ScrollHoriz) || (curOp == Scroll2D)) {
			// look for a change
			int delta = (relativeX - slots[0][cur].x) >> 3;
			if (delta) {
				relativeX = slots[0][cur].x;
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
		
		int dx = slots[0][cur].x - relativeX;
		int dy = slots[0][cur].y - relativeY;
		relativeX = slots[0][cur].x;
		relativeY = slots[0][cur].y;
		
		//acceleration
		int k;
		if ((std::abs(dx) <= accelDist1) || (std::abs(dy) <= accelDist1)) {
			k = accelFactor1;
		} else if ((std::abs(dx) <= accelDist2) || (std::abs(dy) <= accelDist2)) {
			k = accelFactor2;
		} else {
			k = accelFactor3;
		}
		cursorX = cursorX + dx * k;
		cursorY = cursorY + dy * k;

		// limit motion to screen boundaries
		if (cursorX > maxX)
			cursorX = maxX;
		if (cursorX < 0)
			cursorX = 0;
		if (cursorY > maxY)
			cursorY = maxY;
		if (cursorY < 0)
			cursorY = 0;

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
	}  else if (curOp == DoubleClick) {
		// send one more click, so system "Double Click Time" setting 
		// should be longer than timeoutHandler intervar + time for tapping 
		eo.set(EventTypeCode(EV_KEY, BTN_LEFT), 1);
		eo.sync();
		eo.set(EventTypeCode(EV_KEY, BTN_LEFT), 0);
		eo.sync();
		curOp = None;
	}
}

void MtTranslate::logstate() const {
	static const char *opstr[DoubleClick+1] = {
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
		"Scroll2D",
		"DoubleClick"
	};
	// prevously kept writing over the same line
	std::cout /* << '\r' */ << std::setw(12) << opstr[curOp] << ' ' <<
	std::setw(3) << relativeX << ", " << std::setw(3) << relativeY << "  " <<
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
