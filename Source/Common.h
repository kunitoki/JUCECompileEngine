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

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "../../JUCE/extras/Projucer/Source/Utility/jucer_PresetIDs.h"
#include "../../JUCE/extras/Projucer/Source/LiveBuildEngine/projucer_MessageIDs.h"

#include <iostream>
#include <memory>
#include <string>
#include <mutex>

//==============================================================================
#define LOG(x) do { String s; s << x; s << newLine; Logger::writeToLog(s); } while(false)

//==============================================================================
extern "C"
{
    typedef void* LiveCodeBuilder;
    typedef bool (*SendMessageFunction) (void* userInfo, const void* data, size_t dataSize);
    typedef void (*CrashCallbackFunction) (const char* crashDescription);
    typedef void (*QuitCallbackFunction)();
    typedef void (*SetPropertyFunction) (const char* key, const char* value);
    typedef void (*GetPropertyFunction) (const char* key, char* value, size_t size);
    typedef void (*LoginCallbackFunction) (void* userInfo, const char* errorMessage, const char* username, const char* apiKey);

	extern CrashCallbackFunction crashCallbackFunction;
	extern QuitCallbackFunction quitCallbackFunction;
	extern SetPropertyFunction setPropertyFunction;
	extern GetPropertyFunction getPropertyFunction;
	extern bool runningAsChildProcess;
	extern bool loggedIn;
}

//==============================================================================
static inline String concatenateListOfStrings (const StringArray& s)
{
    return s.joinIntoString ("\x01");
}

static inline StringArray separateJoinedStrings (const String& s)
{
    return StringArray::fromTokens (s, "\x01", juce::StringRef());
}
