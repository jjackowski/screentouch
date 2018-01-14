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
#ifndef POLLER_HPP
#define POLLER_HPP

#include <sys/epoll.h>
#include <boost/exception/info.hpp>
#include <boost/noncopyable.hpp>
#include <mutex>
#include <map>
#include <memory>

struct PollerError : virtual std::exception, virtual boost::exception { };
struct PollerCreateError : PollerError { };

/**
 * Responds to a poll event. The associated file descriptor(s) should not be
 * closed until after the response entry is removed from the poller (see
 * Poller::remove()). A class stored in a std::shared_ptr is used instead of
 * std::function because a copy needs to be made and std::shared_ptr limits
 * the size of the copy while std::function may need dynamically allocated
 * data.
 */
class PollResponse {
public:
	/**
	 * Called by Poller::wait(std::chrono::milliseconds) when an event occurs
	 * on the given file descriptor. The PollResponse object may be associated
	 * with multiple file descriptors.
	 * @param fd  The file descriptor with an event.
	 */
	virtual void respond(int fd) = 0;
};

typedef std::shared_ptr<PollResponse>  PollResponseShared;

/**
 * A simple C++ interface to using Linux's epoll() function.
 * This class is thread-safe, but is intended for handling events on one
 * thread at a time.
 *
 * File descriptors are not managed by this class. They must be usable if given
 * to add(). Once give to add(), file descriptors must not be closed until
 * after given to remove(), or the Poller has been destructed. The Poller does
 * not take responsibility for this, or for closing the descriptors.
 *
 * @todo  Improve error handling. Currently, the error code is provided in 
 *        an exception, but it should be translated into an error class that 
 *        indicates the problem.
 *
 * @author  Jeff Jackowski
 */
class Poller : boost::noncopyable {
	/**
	 * Holds responders keyed by their file descriptor.
	 */
	std::map<int, PollResponseShared> things;
	/**
	 * Used to allow for thread-safe operation.
	 */
	mutable std::mutex block;
	/**
	 * The file descriptor provided by epoll_create().
	 */
	int epfd;
public:
	Poller();
	~Poller();
	/**
	 * Adds a file descriptor to check for events.
	 * @pre           The file descriptor is not already added to this poller.
	 * @param prs     A shared pointer to the object that will be informed when
	 *                an event on the file descriptor occurs. The same object
	 *                may be used with multiple file descriptors.
	 * @param fd      The file descriptor.
	 * @param events  See the
	 *                [documentation for epoll_ctl() and epoll_event::events.](http://man7.org/linux/man-pages/man2/epoll_ctl.2.html)
	 * @warning  This function will block if wait(std::chrono::milliseconds) is
	 *           waiting on events to occur.
	 */
	void add(const PollResponseShared &prs, int fd, int events = EPOLLIN);
	/**
	 * Returns the PollResponseShared object associated with the given file
	 * descriptor by a previous call to add().
	 * @param fd      The file descriptor.
	 * @return        The PollResponseShared object. It will be empty if the
	 *                file descriptor wasn't added, or if it was removed since
	 *                it was last added.
	 * @warning  This function will block if wait(std::chrono::milliseconds) is
	 *           waiting on events to occur.
	 */
	PollResponseShared get(int fd) const;
	/**
	 * Removes the entry for the given file descriptor, along with the
	 * associated PollResponseShared object.
	 * @param fd      The file descriptor.
	 * @warning  This function will block if wait(std::chrono::milliseconds) is
	 *           waiting on events to occur.
	 */
	PollResponseShared remove(int fd);
	/**
	 * Waits up to the specified time for events, and processes events
	 * immediately. Up to 32 events may be recorded in a single call; more
	 * are not because it would add complication to handling errors from the
	 * second and subsequent calls to epoll_wait() since there would already be
	 * events awaiting processing.
	 *
	 * @warning    Access to the internal member @a things is blocked until all
	 *             events are locally recorded, and is unblocked before the
	 *             PollResponse::respond() functions are called. This will
	 *             cause calls to add(), remove(), and get(), to block while
	 *             wait() is waiting on events to occur.
	 *
	 * After the events are recorded into a local data structure, the internal
	 * member @a things is no longer used by this function. The
	 * PollResponseShared objects that are needed for the events that occured
	 * are copied as part of recording the events. The copies are used to call
	 * the PollResponse::respond() functions. This means that calls to add()
	 * and remove() do not affect the processing of the locally recorded events.
	 *
	 * The PollResponse::respond() functions are called in the order that the
	 * associated events were reported by epoll_wait().
	 *
	 * @param timeout  The maximum amount of time to wait for events to occur.
	 *                 The function will begin processing events as soon as they
	 *                 are available, and returns after processing. A value of
	 *                 zero will handle events that are already queued without
	 *                 waiting for more. A value of -1 will wait indefinitely.
	 * @return   The number of events handled. If zero, the function waited the
	 *           maximum amount of time.
	 */
	int wait(std::chrono::milliseconds timeout);
	/**
	 * Waits indefinitely for events, only returning after an event is handled.
	 * Same as calling wait(std::chrono::milliseconds(-1)).
	 * @sa wait(std::chrono::milliseconds).
	 */
	int wait() {  // indefinite
		return wait(std::chrono::milliseconds(-1));
	}
	/**
	 * Handles events that are already waiting. Same as calling
	 * wait(std::chrono::milliseconds(0)).
	 * @sa wait(std::chrono::milliseconds).
	 */
	int check() { // no block
		return wait(std::chrono::milliseconds(0));
	}
};

#endif        //  #ifndef POLLER_HPP
