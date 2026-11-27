//
// LogCapture.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "LogCapture.h"
#include "Poco/Message.h"
#include "Poco/Mutex.h"
#include "Poco/Logger.h"
#include <chrono>
#include <vector>
#include <cstddef>
#include <string>


void CaptureChannel::log(const Poco::Message& msg)
{
	Poco::FastMutex::ScopedLock lock(_mutex);
	_messages.push_back(msg);
	_logged.set();
}


std::vector<Poco::Message> CaptureChannel::waitForMessages(std::size_t count)
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	for (;;)
	{
		{
			Poco::FastMutex::ScopedLock lock(_mutex);
			if (_messages.size() >= count) return _messages;
		}
		const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
		if (remaining.count() <= 0) break;
		_logged.tryWait(static_cast<long>(remaining.count()));
	}
	Poco::FastMutex::ScopedLock lock(_mutex);
	return _messages;
}


LogCapture::LogCapture(const std::string& loggerName):
	_logger(Poco::Logger::get(loggerName)),
	_pPreviousChannel(_logger.getChannel()),
	_previousLevel(_logger.getLevel()),
	_pChannel(new CaptureChannel)
{
	_logger.setChannel(_pChannel);
	_logger.setLevel(Poco::Message::PRIO_INFORMATION);
}


LogCapture::~LogCapture()
{
	_logger.setChannel(_pPreviousChannel);
	_logger.setLevel(_previousLevel);
}
