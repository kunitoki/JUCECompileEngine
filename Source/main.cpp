/*
 ==============================================================================

 - JUCECompileEngine - Copyright (c) 2016, Lucio Asnaghi
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 3. Neither the name of the copyright holder nor the names of its contributors
 may be used to endorse or promote products derived from this software without
 specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 OF THE POSSIBILITY OF SUCH DAMAGE.

 ==============================================================================
 */

#include "Common.h"
#include "LiveCodeBuilder.h"

//==============================================================================
extern "C" {

CrashCallbackFunction crashCallbackFunction = nullptr;
QuitCallbackFunction quitCallbackFunction = nullptr;
SetPropertyFunction setPropertyFunction = nullptr;
GetPropertyFunction getPropertyFunction = nullptr;
bool runningAsChildProcess = false;
bool loggedIn = true;

//==============================================================================
JUCE_API void projucer_sendMessage(LiveCodeBuilder lcb,
								   const void* messageData,
								   size_t messageDataSize)
{
	jassert(lcb != nullptr);

	LiveCodeBuilderImpl* liveCodeBuilder = static_cast<LiveCodeBuilderImpl*>(lcb);

	ValueTree message = ValueTree::readFromData(messageData, messageDataSize);

	if (message.getType() == MessageTypes::BUILDINFO)
	{
		LOG("BUILDINFO");
		LOG(message.toXmlString());

		liveCodeBuilder->setBuildInfo(message);
	}
	else if (message.getType() == MessageTypes::LIVE_FILE_UPDATE)
	{
		LOG("LIVE_FILE_UPDATE");
		LOG(message.toXmlString());

		liveCodeBuilder->fileUpdated(message.getProperty("file").toString(),
									 message.getProperty("text").toString());
	}
	else if (message.getType() == MessageTypes::LIVE_FILE_CHANGES)
	{
		LOG("LIVE_FILE_CHANGES");
		LOG(message.toXmlString());

		Array<LiveCodeChange> changes;
		for (int i = 0; i < message.getNumChildren(); i++)
		{
			ValueTree child = message.getChild(i);
			if (child.hasType(MessageTypes::CHANGE))
			{
				LiveCodeChange change;
				change.start = (int)child.getProperty("start", 0);
				change.end = (int)child.getProperty("end", 0);
				change.text = child.getProperty("text", "");
				changes.add(change);
			}
		}

		liveCodeBuilder->fileChanged(message.getProperty("file").toString(),
									 changes);
	}
	else if (message.getType() == MessageTypes::LIVE_FILE_RESET)
	{
		LOG("LIVE_FILE_RESET");
		LOG(message.toXmlString());

		liveCodeBuilder->fileReset(message.getProperty("file").toString());
	}
	else if (message.getType() == MessageTypes::CLEAN_ALL)
	{
		LOG("CLEAN_ALL");
		LOG(message.toXmlString());

		liveCodeBuilder->cleanAll();
	}
	else if (message.getType() == MessageTypes::RELOAD)
	{
		LOG("RELOAD");
		LOG(message.toXmlString());

		liveCodeBuilder->reloadComponents();
	}
	else if (message.getType() == MessageTypes::OPEN_PREVIEW)
	{
		LOG("OPEN_PREVIEW");
		LOG(message.toXmlString());
	}
	else if (message.getType() == MessageTypes::LAUNCH_APP)
	{
		LOG("LAUNCH_APP");
		LOG(message.toXmlString());

		liveCodeBuilder->launchApp();
	}
	else if (message.getType() == MessageTypes::FOREGROUND)
	{
		LOG("FOREGROUND");
		LOG(message.toXmlString());

		liveCodeBuilder->foregroundProcess((int)message.getProperty("parentActive") == 1);
	}
	else if (message.getType() == MessageTypes::PING)
	{
		LOG("PING");
		LOG(message.toXmlString());

		liveCodeBuilder->pong();
	}
	else if (message.getType() == MessageTypes::QUIT_SERVER)
	{
		LOG("QUIT_SERVER");
		LOG(message.toXmlString());

		//liveCodeBuilder->pong();
	}
	else
	{
		LOG("projucer_sendMessage");
		LOG(message.toXmlString());
	}
}

//==============================================================================
JUCE_API void projucer_initialise(CrashCallbackFunction crashCallback,
								  QuitCallbackFunction quitCallback,
								  SetPropertyFunction setPropertyCallback,
								  GetPropertyFunction getPropertyCallback,
								  bool setupSignals)
{
	LOG("projucer_initialise setupSignals:" << setupSignals);

	crashCallbackFunction = crashCallback;
	quitCallbackFunction = quitCallback;
	setPropertyFunction = setPropertyCallback;
	getPropertyFunction = getPropertyCallback;
	runningAsChildProcess = setupSignals;

	if (runningAsChildProcess)
	{
#if !defined(JUCE_WINDOWS)
		::signal(SIGPIPE, SIG_IGN);
#endif
	}
}

JUCE_API void projucer_shutdown()
{
	LOG("projucer_shutdown");
}

//==========================================================================
JUCE_API LiveCodeBuilder projucer_createBuilder(SendMessageFunction sendFunction,
												void* userInfo,
												const char* projectID,
												const char* cacheFolder)
{
	LOG("projucer_createBuilder " << projectID << " " << cacheFolder);

	ScopedPointer<LiveCodeBuilderImpl> liveCodeBuilder(new LiveCodeBuilderImpl(sendFunction,
																			   userInfo,
																			   projectID,
																			   cacheFolder));

	Logger::setCurrentLogger(new FileLogger(File(cacheFolder).getChildFile("live.log"),
											"Welcome to unofficial Live Code Builder 4 Projucer"));

	return (void*)liveCodeBuilder.release();
}

JUCE_API void projucer_deleteBuilder(LiveCodeBuilder lcb)
{
	LOG("projucer_deleteBuilder");

	if (lcb != nullptr)
	{
		delete static_cast<LiveCodeBuilderImpl*>(lcb);
	}
}

//==============================================================================
JUCE_API int projucer_getVersion()
{
	// LOG("projucer_getVersion");
	return 1;
}

JUCE_API void projucer_login(const char* userLoginName,
							 const char* userPassword,
							 bool remainLoggedIn,
							 LoginCallbackFunction loginCallback,
							 void* callbackUserInfo)
{
	LOG("projucer_login " << userLoginName << " " << userPassword);

	loggedIn = true;

	loginCallback(callbackUserInfo, nullptr, "JohnDoe", "FN3WA2pmt4");
}
    
JUCE_API void projucer_logout()
{
	LOG("projucer_logout");

	loggedIn = false;
}
    
JUCE_API bool projucer_isLoggedIn()
{
    return loggedIn;
}
    
JUCE_API void projucer_getLoginName(char* loginName)
{
    //LOG("projucer_getLoginName");
	char loginNameLocal[32];
	memset(loginNameLocal, 0, 32);
	snprintf(loginNameLocal, 31, "%s", "JohnDoe");

	loginName = &loginNameLocal[0];
}
    
JUCE_API bool projucer_hasLicense(const char* featureName)
{
	//LOG("projucer_hasLicense " << featureName);
    return true;
}

JUCE_API bool projucer_hasLiveCodingLicence()
{
    //LOG("projucer_hasLiveCodingLicence");
    return true;
}

} // extern "C"
