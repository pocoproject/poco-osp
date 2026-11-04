//
// LogCapture.h
//
// Definition of the CaptureChannel and LogCapture classes.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef LogCapture_INCLUDED
#define LogCapture_INCLUDED


#include "Poco/Channel.h"
#include "Poco/AutoPtr.h"
#include "Poco/Event.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include "Poco/Mutex.h"
#include <cstddef>
#include <string>
#include <vector>


class CaptureChannel: public Poco::Channel
	/// Keeps every message logged to it.
{
public:
	using Ptr = Poco::AutoPtr<CaptureChannel>;

	CaptureChannel() = default;
		/// Creates the channel without messages.

	CaptureChannel(const CaptureChannel&) = delete;
	CaptureChannel(CaptureChannel&&) = delete;
	CaptureChannel& operator = (const CaptureChannel&) = delete;
	CaptureChannel& operator = (CaptureChannel&&) = delete;

	void log(const Poco::Message& msg) override;

	[[nodiscard]] std::vector<Poco::Message> waitForMessages(std::size_t count);
		/// Returns the messages as soon as at least count have been logged,
		/// or after ten seconds.

protected:
	~CaptureChannel() override = default;
		/// Reference counted: destroyed through release() only.

private:
	Poco::FastMutex _mutex;
	Poco::Event _logged;
	std::vector<Poco::Message> _messages;
};


class LogCapture
	/// Sends the messages of a logger to a CaptureChannel while the object exists.
{
public:
	LogCapture() = delete;

	explicit LogCapture(const std::string& loggerName);
		/// Sends the messages of the logger with the given name, from the
		/// information level up, to a new CaptureChannel.

	~LogCapture();
		/// Restores the channel and the level of the logger.

	LogCapture(const LogCapture&) = delete;
	LogCapture(LogCapture&&) = delete;
	LogCapture& operator = (const LogCapture&) = delete;
	LogCapture& operator = (LogCapture&&) = delete;

	[[nodiscard]] CaptureChannel& channel();
		/// Returns the channel that receives the messages.

private:
	Poco::Logger& _logger;
	Poco::Channel::Ptr _pPreviousChannel;
	int _previousLevel;
	CaptureChannel::Ptr _pChannel;
};


//
// inlines
//
inline CaptureChannel& LogCapture::channel()
{
	return *_pChannel;
}


#endif // LogCapture_INCLUDED
