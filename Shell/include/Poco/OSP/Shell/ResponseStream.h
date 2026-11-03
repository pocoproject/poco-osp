//
// ResponseStream.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  ResponseStream
//
// Definition of the ResponseStream class and friends.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_ResponseStream_INCLUDED
#define OSP_Shell_ResponseStream_INCLUDED


#include "Poco/OSP/Shell/Shell.h"
#include "Poco/UnbufferedStreamBuf.h"
#include <ostream>


namespace Poco::OSP::Shell {


class OSPShell_API ResponseStreamBuf: public Poco::UnbufferedStreamBuf
	/// This stream wraps all lines sent through it in
	/// SMTP (RFC 2821) format.
	///
	/// Lines are grouped together in blocks, which every block having
	/// the format of an RFC 2821 server response message.
	///
	/// The block begins with a three-digit status code, followed by
	/// either a minus sign ('-') if more lines follow, or a space
	/// character (' '), if the line is the only line in this block.
	/// All lines but the last line in a block also start with a
	/// three-digit status code and a minus character. The last line
	/// starts with a three-digit status code, a space character and
	/// the line data.
	///
	/// Examples:
	///     100-This is a multiline
	///     100-message. The message
	///     100 has three lines in total.
	///
	///     200 OK
{
public:	
	ResponseStreamBuf(std::ostream& ostr, int status);
		/// Creates the ResponseStreamBuf and connects it
		/// to the given output stream.

	~ResponseStreamBuf() override;
		/// Destroys the ResponseStreamBuf.

	ResponseStreamBuf() = delete;
		
	int sync() override;
		/// Flushes the currently buffered line and ends the
		/// response block.	
		
	void setStatus(int status);
		/// Sets the status code that is prepended to response lines.
	
	[[nodiscard]] int getStatus() const;
		/// Returns the status code that is prepended to response lines.
	
protected:
	int writeToDevice(char c) override;
	void flushLine(bool more);
	
private:
	std::ostream& _ostr;
	int _status;
	std::string _line;
	bool _lineComplete = false;
};


class OSPShell_API ResponseIOS: public virtual std::ios
	/// The base class for ResponseStream.
{
public:
	ResponseIOS(std::ostream& ostr, int status);
		/// Creates the ResponseIOS using the given output stream and status.
		
	~ResponseIOS() override;
		/// Destroys the ResponseIOS.

	ResponseIOS() = delete;
		
	[[nodiscard]] ResponseStreamBuf* rdbuf();
		/// Returns a pointer to the underlying streambuf.
		
	void setStatus(int status);
		/// Sets the response status code.
		
	[[nodiscard]] int getStatus() const;
		/// Returns the response status code.
		
protected:
	ResponseStreamBuf _buf;
};


class OSPShell_API ResponseStream: public ResponseIOS, public std::ostream
	/// An output stream, based on ResponseStreamBuf.
{
public:
	ResponseStream(std::ostream& ostr, int status = 0);
		/// Creates the ResponseStream, using the given output
		/// stream and status.
		
	~ResponseStream() override;
		/// Destroys the ResponseStream.
};


//
// inlines
//
inline int ResponseStreamBuf::getStatus() const
{
	return _status;
}


inline void ResponseIOS::setStatus(int status)
{
	_buf.setStatus(status);
}


inline int ResponseIOS::getStatus() const
{
	return _buf.getStatus();
}


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_ResponseStream_INCLUDED
