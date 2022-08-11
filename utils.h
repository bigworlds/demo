#pragma once

#include <Windows.h>
#include <unordered_map>
#include <malloc.h>
#include <process.h>
#include <atomic>
#include <queue>
#include <thread>
#include <cassert>
#include <functional>
#include "Fiber.h"
#include "glm/glm.hpp"
#include "mmgr.h"

//http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
template<typename T>
class mpmc_bounded_queue
{
public:
	mpmc_bounded_queue(size_t buffer_size)
		: buffer_(new cell_t[buffer_size])
		, buffer_mask_(buffer_size - 1)
	{
		assert((buffer_size >= 2) &&
			((buffer_size & (buffer_size - 1)) == 0));
		for (size_t i = 0; i != buffer_size; i += 1)
			buffer_[i].sequence_.store(i, std::memory_order_relaxed);
		enqueue_pos_.store(0, std::memory_order_relaxed);
		dequeue_pos_.store(0, std::memory_order_relaxed);
	}

	~mpmc_bounded_queue()
	{
		delete[] buffer_;
	}

	bool enqueue(T const& data)
	{
		cell_t* cell;
		size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
		for (;;)
		{
			cell = &buffer_[pos & buffer_mask_];
			size_t seq =
				cell->sequence_.load(std::memory_order_acquire);
			intptr_t dif = (intptr_t)seq - (intptr_t)pos;
			if (dif == 0)
			{
				if (enqueue_pos_.compare_exchange_weak
				(pos, pos + 1, std::memory_order_relaxed))
					break;
			}
			else if (dif < 0)
				return false;
			else
				pos = enqueue_pos_.load(std::memory_order_relaxed);
		}
		cell->data_ = data;
		cell->sequence_.store(pos + 1, std::memory_order_release);
		return true;
	}

	bool dequeue(T& data)
	{
		cell_t* cell;
		size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
		for (;;)
		{
			cell = &buffer_[pos & buffer_mask_];
			size_t seq =
				cell->sequence_.load(std::memory_order_acquire);
			intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
			if (dif == 0)
			{
				if (dequeue_pos_.compare_exchange_weak
				(pos, pos + 1, std::memory_order_relaxed))
					break;
			}
			else if (dif < 0)
				return false;
			else
				pos = dequeue_pos_.load(std::memory_order_relaxed);
		}
		data = cell->data_;
		cell->sequence_.store
		(pos + buffer_mask_ + 1, std::memory_order_release);
		return true;
	}

private:
	struct cell_t
	{
		std::atomic<size_t>   sequence_;
		T                     data_;
	};

	static size_t const     cacheline_size = 64;
	typedef char            cacheline_pad_t[cacheline_size];

	cacheline_pad_t         pad0_;
	cell_t* const           buffer_;
	size_t const            buffer_mask_;
	cacheline_pad_t         pad1_;
	std::atomic<size_t>     enqueue_pos_;
	cacheline_pad_t         pad2_;
	std::atomic<size_t>     dequeue_pos_;
	cacheline_pad_t         pad3_;

	mpmc_bounded_queue(mpmc_bounded_queue const&);
	void operator = (mpmc_bounded_queue const&);
};












/*small stack size: 64KIB*/
#define FIBER_STACK_SIZE (64*1024)

class JobSystem2;



struct WorkerThread2
{
	JobSystem2* scheduler;
	uint32_t threadId;
	uint32_t dummy;
	HANDLE hWorkerThread;
	HANDLE ThreadKickEvent;

	//이거 안쓰고있었네.. 왜 안쓰고 있었지?
	//threadFunc에 while()루프에 쓰려고 했었나??
	//HANDLE ThreadStopEvent;

	///A thread is the execution unit, the fiber is the context.
	//쓰레드에 잡파이버를 실행시키려면 쓰레드 파이버(메인컨텍스트)에서 잡파이버로 스위치해야됨
	Fiber thread_context;

};

unsigned __stdcall workerThreadLoop(LPVOID lpParam);
using JobCallback2 = void(*)(void* userdata);
using TaskCallback = void(*)(void* param);

struct JobDeclaration2
{
	JobDeclaration2() 
	{
		callback = nullptr;
		pUserdata = nullptr;
		pCounter = nullptr;
	};
	~JobDeclaration2() 
	{
	};

	JobCallback2 callback;
	void* pUserdata;
	std::atomic_uint* pCounter; //shared_ptr?
};

class JobSystem2
{
public:
	//잡큐
	mpmc_bounded_queue<JobDeclaration2>* m_jobQueue;
	std::unordered_map<int, int> m_tidtoHandleMap;
	WorkerThread2* m_workers;

	//파이버인덱스 큐
	//Fiber* m_fiberMagazine;
	//포인터 대신 매거진 핸들을 사용
	mpmc_bounded_queue<uint32_t>* m_fiberQueue;
	std::vector<Fiber> m_fiberMagazine;

private:

	int m_maxHWthreads;
	int m_numWorkerThreads;
	

private:
	JobSystem2(const JobSystem2 &other);//disable copy constructor

public:
	JobSystem2();
	~JobSystem2();
	
public: //method
	void Init_workerThread(int count);
	void Init_fiberMagazine(int count);

	void Init(int numThread, int numFiberMag, int numJobQueue);

	void RunTask(void(*taskCallback)(void *), void* args, std::atomic_uint* cnt);
	void WaitForTask(std::atomic_uint* counter, uint32_t value);
	void RunJobs(JobDeclaration2* to, int numTJobs, std::atomic_uint** counter);
	void WaitForCounter(std::atomic_uint** counter, uint32_t value, Fiber* caller);
	void Endofstory();

};

JobSystem2::JobSystem2()
{

};

JobSystem2::~JobSystem2()
{
	
};

void JobSystem2::Endofstory()
{
	for (int i = 0; i < m_fiberMagazine.size(); ++i)
	{
		Fiber* f = &m_fiberMagazine[i];
		if (fiber_is_executing(f))
		{
			printf("\n %d fiber is running... r u sure?", i);
		}
		fiber_destroy(f);


		//m_fiberQueue
	}

	for (int i = 0; i < m_numWorkerThreads; ++i)
	{
		CloseHandle(m_workers[i].hWorkerThread);
		m_workers[i].hWorkerThread = 0;

		//쓰레드 종료가 안된 상태에서 이벤트를 꺼버리면 안됨
		//workerThreadLoop()에서 킥이벤트를 기다리면서 루프도는데
		//이벤트가 꺼지면 바로 진입해서 에러가 나고있었음
		//while(1)으로 대기타는게 과연 좋은 방법일까?
		//
		//CloseHandle(m_workers[i].ThreadKickEvent);
		//m_workers[i].ThreadKickEvent = 0;
		//CloseHandle(m_workers[i].ThreadStopEvent);
		//m_workers[i].ThreadStopEvent = 0;
		//_endthreadex(0);
		
	}

	delete m_fiberQueue;
	delete m_jobQueue;
	
	printf("\njobsystem exit\n");
	
}


void JobSystem2::Init_workerThread(int count)
{
	assert(count <= m_maxHWthreads);
	m_numWorkerThreads = count;

	m_workers = (WorkerThread2 *)_aligned_malloc(sizeof(WorkerThread2) * m_maxHWthreads, 16);

	for (int i = 0; i < m_numWorkerThreads; ++i)
	{
		m_workers[i].scheduler = this;

		m_workers[i].ThreadKickEvent = CreateEvent(
			NULL,
			FALSE, //manual reset
			FALSE, //init state
			NULL); //이름붙이고 싶으면

		m_workers[i].hWorkerThread = (HANDLE)_beginthreadex(
			NULL,
			512 * 1024,//stacksize
			workerThreadLoop,
			&m_workers[i],
			0, //CREATE_SUSPENDED?
			&m_workers[i].threadId//thrdaddr; GetThreadId(h) 로 얻을 수 있는거
		);

		assert(m_workers[i].hWorkerThread);

		m_tidtoHandleMap[m_workers[i].threadId] = i;


		fiber_init_toplevel(&m_workers[i].thread_context);
	}
}

void guard_cleanup2(Fiber* self, void* null)
{
	printf("\n sth wrong\n");
	abort();
}

void JobSystem2::Init_fiberMagazine(int count)
{
	m_fiberMagazine.resize(count);
	m_fiberQueue = new mpmc_bounded_queue<uint32_t>(count);
	for (int i = 0; i < count; ++i)
	{
		Fiber* next = &m_fiberMagazine[i];
		fiber_alloc(next, FIBER_STACK_SIZE, guard_cleanup2, nullptr, FIBER_FLAG_GUARD_LO);
		
		m_fiberQueue->enqueue(i);
	}
}

void JobSystem2::Init(int numThread, int numFiberMag, int numJobQueue)
{
	m_maxHWthreads = std::thread::hardware_concurrency();

	Init_workerThread(numThread);

	Init_fiberMagazine(numFiberMag);

	m_jobQueue = new mpmc_bounded_queue<JobDeclaration2>(numJobQueue);

}

struct FiberArgs
{
	Fiber* caller;
	Fiber* self; //callee
	void* pUserdata;
};


void fiberStart(void* param)
{
	FiberArgs* p = (FiberArgs *)param;
	JobSystem2* sched = (JobSystem2*)p->pUserdata;
	//QueueSPMC<JobDeclaration2>* jq = &sched->m_jobQueue;
	mpmc_bounded_queue<JobDeclaration2>* jq = sched->m_jobQueue;


	/*20200211
	스트레스 테스트중 잘 되다가 중간에 랜덤하게 멈추는 현상을 관측
	잡큐 numObjects카운터가 제대로 작동하고 있지 않다
	numObjects를 확인해서 팝을 하게 되는데
	numObjects확인할때 1 이던게 팝할때는 0으로 되는것 같음.
	짐작컨데 SPMC 인게 문제같음
	Single Produce인데 지금같은 경우엔 여러 파이버에서 잡을 넣고 있는 상황이다.
	mpmc큐로 교체해서 픽스
	*/

	//if(atomic_load(jq->numObjects))
	{
		//assert(atomic_load(jq->numObjects) > 0);
		//JobDeclaration2 jb = jq->pop();
		JobDeclaration2 jb = {};
		if (jq->dequeue(jb))
		{
			p->pUserdata = jb.pUserdata; //새로운 arg로 교체하는게 쫌 이상하긴한데. 
			jb.callback(p);
			//만약 함수 호출이 실패한다면? 잡을 다시 집어넣어줘야 할까??

			atomic_fetch_sub(jb.pCounter, 1);
			assert(atomic_load(jb.pCounter) != uint32_t(-1));
		}

	}

	//파이버는 그냥 리턴하면 안되고(guard_cleanup2()함수를 호출하게됨) 스위치 백 해줘야함
	fiber_switch(p->self, p->caller);
}

unsigned __stdcall workerThreadLoop(LPVOID lpParam)
{
	DWORD ret = 0;
	WorkerThread2* t = (WorkerThread2*)lpParam;
	JobSystem2* sched = t->scheduler;


	//이럴게 아니라 stop이벤트를 기다려야 할것 같음
	while (1)
	{
		ret = WaitForSingleObject(t->ThreadKickEvent, INFINITE);
		
		int who = sched->m_tidtoHandleMap[GetCurrentThreadId()];

		/*
		실행할 파이버가 없는데-그럼 freeFiberidx = 0 이됨- 0번 파이버에다 또 실어서 실행하는 바람에 에러가 있었음
		실행할 잡이 있는지를 확인해 봐야하지 않나??
		*/
		
		
		
		FiberArgs payload = {};
		payload.caller = &sched->m_workers[who].thread_context;
		uint32_t freeFiberidx = 0;
		if (sched->m_fiberQueue->dequeue(freeFiberidx))
		{
			Fiber* freeFiber = &sched->m_fiberMagazine[freeFiberidx];
			payload.self = freeFiber;
			payload.pUserdata = sched;

			fiber_push_return(payload.self, fiberStart, &payload, sizeof(FiberArgs));

			fiber_switch(payload.caller, payload.self);
		}
		else
		{
			//printf("\n 실행할 파이버가 없음\n");
			SwitchToThread();
		}
		
		

	}

	
	return 1;
}



void JobSystem2::RunTask(void(*taskJob)(void * args), void* args, std::atomic_uint* counter)
{

	//20200204
	//음? 태스크나 태스크가 실행할 잡이나 어차피 다 똑같은 잡인데 따로 메인파이버로 실행하기보단 걍 잡큐에 넣어버릴까?
	JobDeclaration2 jobDecls = {};
	jobDecls.callback = taskJob;
	jobDecls.pUserdata = args; //args가 어떤 타입일지는 실행되는 태스크에 따라 결정됨
	//카운터를 밖에서 얻어와봤음.. 
	jobDecls.pCounter = counter;
	if (m_jobQueue->enqueue(jobDecls))
	{
		//여기서 킥할수도
		for (int i = 0; i < m_numWorkerThreads; ++i)
		{
			SetEvent(m_workers[i].ThreadKickEvent);
		}
	}
	else
	{
		assert(0);
	}

}

void JobSystem2::WaitForTask(std::atomic_uint* counter, uint32_t value)
{
	//20200404
	//파이버에서 기다리는게 아니라 워커스레드에서 기다리게 되는 경우에 쓰려고함
	
	volatile uint32_t c = std::atomic_load(counter);


	/*20210408
	적절히 깨울 방법이 필요한데
	너무 많이 깨우니까 파이버를 전부 소모하게되는것 같음
	workerThreadLoop() 루틴에 "실행할 파이버가 없음" 이 뜨는 원인
	한번만 깨울까

	스케줄러를 만들려면 워커쓰레드를 대기시켰다가 일감이 들어오면 돌리는 알고리즘이 필요하다
	원래 참고한 파이버 구현은 한번만 실행하고 마는거라 이 부품은 우리가 만들어야함
	파이버 스위칭은 문제가 없는데 프리파이버를 관리하는부분이 문제임

	https://github.com/simonfxr/fiber
	https://github.com/RichieSams/FiberTaskingLib/blob/master/source/task_scheduler.cpp#L94
	*/
	for (int i = 0; i < m_numWorkerThreads; ++i)
	{
		//runTask()에서 킥하도록 해볼까
		//SetEvent(m_workers[i].ThreadKickEvent);
	}
	while (c > value)
	{
		
		c = atomic_load(counter);
	}
}

void JobSystem2::RunJobs(JobDeclaration2* jobs, int numJobs, std::atomic_uint** counter)
{
	atomic_store(*counter, numJobs);

	for (int i = 0; i < numJobs; ++i)
	{
		jobs[i].pCounter = *counter;
		if (m_jobQueue->enqueue(jobs[i]))
		{
		}
		else
		{
			assert(0);
		}
	}
}

void JobSystem2::WaitForCounter(std::atomic_uint** counter, uint32_t value, Fiber* caller)
{
	//int who = tidtoHandleMap[GetCurrentThreadId()];

	while (atomic_load(*counter) > value)
	{
		//20220810
		// 이렇게 하면 안될것 같음 더 느려짐 
#if 0
		
		//프리 파이버 팝
		uint32_t freeFiberidx = 0;
		if (m_fiberQueue->dequeue(freeFiberidx))
		{
			Fiber* freeFiber = &m_fiberMagazine[freeFiberidx];

			FiberArgs payload = {};
			payload.caller = caller;
			payload.self = freeFiber;
			payload.pUserdata = this; //'this' jobsystem

			fiber_push_return(payload.self, fiberStart, &payload, sizeof(FiberArgs));

			//switch
			fiber_switch(payload.caller, payload.self);

			//스위치백
			/*20200210 트러블슈팅 freeFiber->regs.sp 
			//파이버 스택을 크게 잡아주니까 오래 버티는걸로 봐선 sp를 리셋해줘야됨..
			//애초에 우리가 참고한 파이버 디자인이 한번 쓰고 버리는 용도라서 그런지 
			//풀에 만들어 놓고 재활용하는데 적합하지 않은 문제가 있다
			//realloc 할 방법이 필요함
			*/
			fiber_destroy(freeFiber);
			fiber_alloc(freeFiber, FIBER_STACK_SIZE, guard_cleanup2, nullptr, FIBER_FLAG_GUARD_LO);

			m_fiberQueue->enqueue(freeFiberidx);
		}
#endif

		//better idea?
		for (int i = 0; i < m_numWorkerThreads; ++i)
		{
			SetEvent(m_workers[i].ThreadKickEvent);
		}

	}

	//all job has done. 
	//return; 

}

