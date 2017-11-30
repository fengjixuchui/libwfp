// cli.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "libwfp/filterengine.h"
#include "libwfp/sessionenumerator.h"
#include "libcommon/string.h"
#include <iostream>
#include <memory>

bool session_callback(const FWPM_SESSION0 &session)
{
	std::wcout << "Session" << std::endl;

	std::wcout << "  key:\t\t\t" << common::string::FormatGuid(session.sessionKey) << std::endl;
	std::wcout << "  name:\t\t\t" <<
		(session.displayData.name == nullptr ? L"n/a" : session.displayData.name) << std::endl;
	std::wcout << "  description:\t\t" <<
		(session.displayData.description == nullptr ? L"n/a" : session.displayData.description) << std::endl;
	std::wcout << "  flags:\t\t" << session.flags << std::endl;
	std::wcout << "  wait timeout:\t\t" << session.txnWaitTimeoutInMSec << std::endl;
	std::wcout << "  sid:\t\t\t" << common::string::FormatSid(*session.sid) << std::endl;
	std::wcout << "  username:\t\t" << session.username << std::endl;
	std::wcout << "  kernel:\t\t" <<
		(session.kernelMode ? L"true" : L"false") << std::endl;

	return true;
}

int main()
{
	std::shared_ptr<FilterEngine> engine(new FilterEngine(false));

	SessionEnumerator::enumerate(engine, session_callback);

    return 0;
}


