//
// ResponseStream.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  ResponseStream
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Shell/ResponseStream.h"
#include "Poco/NumberFormatter.h"


using Poco::NumberFormatter;


namespace Poco::OSP::Shell {


//
// ResponseStreamBuf
//


ResponseStreamBuf::ResponseStreamBuf(std::ostream& ostr, int status):
	_ostr(ostr),
	_status(status)
{
}


ResponseStreamBuf::~ResponseStreamBuf()
{
	try
	{
		sync();
	}
	catch (...)
	{
	}
}

	
int ResponseStreamBuf::sync()
{
	flushLine(false);
	_ostr.flush();
	return 0;
}

	
void ResponseStreamBuf::setStatus(int status)
{
	if (status != _status)
	{
		flushLine(false);
		_status = status;
	}
}


int ResponseStreamBuf::writeToDevice(char c)
{
	if (_lineComplete)
	{
		flushLine(true);
	}
	if (c == '\n')
	{
		_lineComplete = true;
	}
	else _line += c;
	return charToInt(c);
}


void ResponseStreamBuf::flushLine(bool more)
{
	_lineComplete = _lineComplete || !_line.empty();
	if (_lineComplete)
	{
		_ostr << NumberFormatter::format0(_status, 3) << (more ? '-' : ' ') << _line << "\r\n";
	}
	_line.clear();
	_lineComplete = false;
}


//
// ResponseIOS
//


ResponseIOS::ResponseIOS(std::ostream& ostr, int status):
	_buf(ostr, status)
{
	poco_ios_init(&_buf);
}

	
ResponseIOS::~ResponseIOS() = default;

	
ResponseStreamBuf* ResponseIOS::rdbuf()
{
	return &_buf;
}


//
// ResponseStream
//


ResponseStream::ResponseStream(std::ostream& ostr, int status):
	ResponseIOS(ostr, status),
	std::ostream(&_buf)
{
}

	
ResponseStream::~ResponseStream() = default;


} // namespace Poco::OSP::Shell
