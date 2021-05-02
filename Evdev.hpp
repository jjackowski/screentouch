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
#include <libevdev/libevdev.h>
#include <boost/signals2/signal.hpp>
#include "Poller.hpp"

struct EvdevError : virtual std::exception, virtual boost::exception { };
struct EvdevFileOpenError : EvdevError { };
struct EvdevInitError : EvdevError { };
struct EvdevUnsupportedEvent : EvdevError { };
struct EvdevTypeAddError : EvdevError { };
struct EvdevCodeAddError : EvdevError { };
struct EvdevUInputCreateError : EvdevError { };

typedef boost::error_info<struct Info_EvdevEventType, unsigned int>
	EvdevEventType;
typedef boost::error_info<struct Info_EvdevEventCode, unsigned int>
	EvdevEventCode;
typedef boost::error_info<struct Info_EvdevEventType, std::string>
	EvdevEventTypeName;
typedef boost::error_info<struct Info_EvdevEventCode, std::string>
	EvdevEventCodeName;
typedef boost::error_info<struct Info_EvdevEventValue, std::int32_t>
	EvdevEventValue;

union EventTypeCode {
	struct {
		std::uint16_t type;
		std::uint16_t code;
	};
	std::uint32_t typecode;
	EventTypeCode() = default;
	constexpr EventTypeCode(std::uint16_t t, std::uint16_t c) : type(t), code(c) { }
	const char *typeName() const;
	const char *codeName() const;
};

inline bool operator<(EventTypeCode etc0, EventTypeCode etc1) {
	return etc0.typecode < etc1.typecode;
}

/**
 * Handles getting input from a specific input device.
 * @author  Jeff Jackowski
 */
class Evdev :
	boost::noncopyable,
	public PollResponse,
	public std::enable_shared_from_this<Evdev>
{
public:
	typedef boost::signals2::signal< void(EventTypeCode, std::int32_t) >
		InputSignal;
protected:
	typedef std::map<EventTypeCode, InputSignal>  InputMap;
	InputMap receivers;
	libevdev *dev;
public:
	int fd;
	Evdev(const std::string &path);
	Evdev(Evdev &&e);
	~Evdev();
	Evdev &operator=(Evdev &&old);
	/**
	 * Reads in input events when invoked by the poller.
	 */
	virtual void respond(int fd);
	/**
	 * Reports the name of the device through libevdev_get_name().
	 */
	std::string name() const;
	/**
	 * Attempts to gain exclusive access to the input device.
	 * @return  True if exclusive access was granted.
	 */
	bool grab();
	bool hasEventType(unsigned int et) const;
	bool hasEventCode(unsigned int et, unsigned int ec) const;
	bool hasEvent(EventTypeCode etc) const;
	int numSlots() const;
	int value(unsigned int et, unsigned int ec) const;
	void usePoller(Poller &p);
	boost::signals2::connection inputConnect(
		EventTypeCode etc,
		const InputSignal::slot_type &slot,
		boost::signals2::connect_position at = boost::signals2::at_back
	) {
		return receivers[etc].connect(slot, at);
	}
	/**
	 * Provides information about a specified absolute axis.
	 * @param absEc  The event code for the axis to query. It must be for an
	 *               event of type EV_ABS.
	 * @return  A non-null pointer to the data.
	 * @throw EvdevUnsupportedEvent The requested event lacks the information;
	 *                              either isn't an axis, or axis is unsupported.
	 */
	const input_absinfo *absInfo(unsigned int absEc) const;
};

typedef std::shared_ptr<Evdev>  EvdevShared;
