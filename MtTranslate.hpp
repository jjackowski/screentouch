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
#include <chrono>

/**
 * Multi-touch translator.
 * @author  Jeff Jackowski
 */
class MtTranslate {
	/**
	 * Much less useful than anticipated, but may be more useful if the
	 * locations of each spot matter in a future operation.
	 */
	struct SlotState {
		/**
		 * Tracking ID.
		 */
		int tid;
		int x;
		int y;
		SlotState() : tid(-1) { }
	};
	typedef SlotState StateHist[2];
	/**
	 * The touchscreen input device.
	 */
	EvdevShared evdev;
	/**
	 * The user-input device to which the translated input events are output.
	 */
	EvdevOutput eo;
	/**
	 * Data on each of the "slots", stateful contact points reported by
	 * multi-touch protocol B.
	 */
	std::vector<StateHist> slots;
	typedef std::chrono::steady_clock::time_point  timepoint;
	typedef std::chrono::steady_clock::duration  duration;
	/**
	 * The time when some event occured that may need to be referenced later.
	 * For instance, if the user taps the screen, the time is used in case the
	 * user quickly touches the screen again for a drag operation.
	 */
	timepoint eventtime;
	/**
	 * Index of the most current touch information in the StateHist arrays.
	 */
	int cur;
	/**
	 * Index of the old touch information in the StateHist arrays.
	 */
	int old;
	/**
	 * The currently updating slot from the multi-touch input, protocol B.
	 */
	int slot;
	/**
	 * The number of slots in use, which is the number of contact points.
	 */
	int scnt;
	/**
	 * The number of contacts that the operation is responding to. This may be
	 * different from @a scnt.
	 */
	int cntctCur;
	/**
	 * The value of @a cntctCur at the end of synEvent() the last time the
	 * function ran.
	 */
	int cntctOld;
	/**
	 * The location used for the cursor. It is the previous location for most
	 * of synEvent().
	 */
	int cursorX;
	/**
	 * The location used for the cursor. It is the previous location for most
	 * of synEvent().
	 */
	int cursorY;
	/**
	 * The set of mouse-like input operations that are implemented.
	 */
	enum Operation {
		None,
		ReleaseLeft,
		ReleaseRight,
		ReleaseMiddle,
		DragLeft,
		DragRight,
		DragMiddle,
		MoveCursor,
		ScrollVert,
		ScrollHoriz,
		Scroll2D  // 3-finger scroll; seems to not work with Firefox
	};
	/**
	 * The current mouse-like input operation.
	 */
	int curOp;
	/**
	 * Responds to ABS_MT_SLOT input events.
	 */
	void slotEvent(std::int32_t val);
	/**
	 * Responds to ABS_MT_TRACKING_ID input events.
	 */
	void trackEvent(std::int32_t val);
	/**
	 * Responds to ABS_MT_POSITION_X input events.
	 */
	void xPosEvent(std::int32_t val);
	/**
	 * Responds to ABS_MT_POSITION_Y input events.
	 */
	void yPosEvent(std::int32_t val);
	/**
	 * Responds to SYN_REPORT input events.
	 */
	void synEvent();
	/**
	 * Initialization function called by all constructors.
	 */
	void init();
	/**
	 * The minimum distance an initial contact must move before it is considered
	 * to have moved. Mitigates apparent noise in the location.
	 */
	static constexpr int moveDist = 5;
	/**
	 * A length of time between tap-like contacts of the screen used to
	 * implement different behavior when an operation requires mutlple contacts
	 * over time.
	 */
	static constexpr std::chrono::milliseconds tapTime =
		std::chrono::milliseconds(192);
	/**
	 * Logs to stdout what is going on for debugging.
	 */
	void logstate() const;
public:
	/**
	 * Makes a new input translator using the given device for input.
	 */
	MtTranslate(const EvdevShared &ev);
	/**
	 * Makes a new input translator using the given device for input.
	 */
	MtTranslate(EvdevShared &&ev);
	// disconnect signal functions given to evdev
	~MtTranslate();
	/**
	 * Call to handle single-tap button presses. These occur after the tap
	 * when no other touch input is given. As a result, it cannot be in
	 * synEvent() because there will not be an event.
	 */
	void timeoutHandle();
};
