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
#include <boost/exception/errinfo_errno.hpp>
#include "Poller.hpp"
#include <vector>

Poller::Poller() {
	epfd = epoll_create(1);
	if (epfd < 0) {
		BOOST_THROW_EXCEPTION(PollerCreateError() <<
			boost::errinfo_errno(errno)
		);
	}
}

Poller::~Poller() {
	std::lock_guard<std::mutex> lock(block);
	close(epfd);
}

void Poller::add(const PollResponseShared &prs, int fd, int events) {
	std::lock_guard<std::mutex> lock(block);
	epoll_event event;
	event.events = events;
	event.data.fd = fd;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event)) {
		BOOST_THROW_EXCEPTION(PollerError() <<
			boost::errinfo_errno(errno)
		);
	}
	things[fd] = prs;
}

PollResponseShared Poller::get(int fd) const {
	std::lock_guard<std::mutex> lock(block);
	std::map<int, PollResponseShared>::const_iterator iter = things.find(fd);
	if (iter == things.end()) {
		return PollResponseShared();
	}
	return iter->second;
}

PollResponseShared Poller::remove(int fd) {
	std::lock_guard<std::mutex> lock(block);
	std::map<int, PollResponseShared>::iterator iter = things.find(fd);
	if (iter == things.end()) {
		return PollResponseShared();
	}
	if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr)) {
		BOOST_THROW_EXCEPTION(PollerError() <<
			boost::errinfo_errno(errno)
		);
	}
	PollResponseShared res(std::move(iter->second));
	things.erase(iter);
	return res;
}

struct ResponseRecord {
	PollResponseShared prs;
	int fd;
	ResponseRecord(const PollResponseShared &p, int f) : prs(p), fd(f) { }
};

int Poller::wait(std::chrono::milliseconds timeout) {
	std::vector<ResponseRecord> responders;
	{ // event responses called outside of the lock
		std::lock_guard<std::mutex> lock(block);
		epoll_event events[32];
		int count = epoll_wait(epfd, events, 32, timeout.count());
		if (!count) {
			// all done
			return 0;
		} else if (count < 0) {
			BOOST_THROW_EXCEPTION(PollerError() <<
				boost::errinfo_errno(errno)
			);
		}
		responders.reserve(count);
		for (int loop = 0; loop < count; ++loop) {
			int fd = events[loop].data.fd;
			std::map<int, PollResponseShared>::const_iterator iter =
				things.find(fd);
			if (iter != things.end()) {
				responders.emplace_back(iter->second, fd);
			}
		}
	}
	std::for_each(
		responders.begin(),
		responders.end(),
		[](const ResponseRecord &rr) {
			rr.prs->respond(rr.fd);
		}
	);
	return responders.size();
}
