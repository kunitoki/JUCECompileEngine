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

#include "Common.h"

#include <queue>
#include <mutex>
#include <exception>
#include <condition_variable>

template<typename T>
class SharedQueue
{
public:
    SharedQueue() = default;

    void push(T value)
    {
		{
			std::lock_guard<std::mutex> lock(mutex);
			objectQueue.push(value);
		}

		conditionVariable.notify_one();
	}

	bool tryAndPop(T& returnValue)
    {
		std::lock_guard<std::mutex> lock(mutex);

        if (objectQueue.empty())
        {
			return false;
		}

        returnValue = objectQueue.front();
		objectQueue.pop();

        return true;
	}

	void waitAndPop(T& returnValue)
    {
		std::unique_lock<std::mutex> lock(mutex);

        conditionVariable.wait(lock, [&]() {
            return ! objectQueue.empty();
        });

		returnValue = objectQueue.front();
		objectQueue.pop();
	}

	bool empty() const
    {
		std::lock_guard<std::mutex> lock(mutex);

		return objectQueue.empty();
	}

    std::size_t size() const
    {
		std::lock_guard<std::mutex> lock(mutex);
		return objectQueue.size();
	}

private:
    mutable std::mutex mutex;
    std::queue<T> objectQueue;
    std::condition_variable conditionVariable;
};
