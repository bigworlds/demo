#pragma once
/*
허접 타이머
다음을 참고하시오:
https://msdn.microsoft.com/ko-kr/library/windows/desktop/ms644905(v=vs.85).aspx
The frequency of the performance counter is fixed at system boot and is consistent across all processors. 
Therefore, the frequency need only be queried upon application initialization, and the result can be cached.

이것도 참고:
Microsecond Resolution Time Services For Windows
*/
#include <assert.h>
#include <Windows.h>
#include <stdint.h>

class Timer
{
private:
	uint64_t m_tickAtStart;
	uint64_t m_accumulatedTicks;
	bool m_isRunning;

public:
	Timer();
	~Timer();

	void Start();
	void Stop();
	void Restart();
	double GetElapsedTime();

	static uint64_t InitTicksPerSecond();

	static uint64_t GetTickCount()
	{
		LARGE_INTEGER i;
		QueryPerformanceCounter(&i);
		return i.QuadPart;
	}

	static uint64_t GetTickPerSecond()
	{
		static uint64_t tickPerSecond = InitTicksPerSecond();
		return tickPerSecond;
	}

	static double TicksToSecond( uint64_t ticks)
	{
		return double(ticks) / double(GetTickPerSecond());
	}

};
