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

	//�̰� �Ⱦ����־���.. �� �Ⱦ��� �־���?
	//threadFunc�� while()������ ������ �߾���??
	//HANDLE ThreadStopEvent;

	///A thread is the execution unit, the fiber is the context.
	//�����忡 �����̹��� �����Ű���� ������ ���̹�(�������ؽ�Ʈ)���� �����̹��� ����ġ�ؾߵ�
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
	//��ť
	mpmc_bounded_queue<JobDeclaration2>* m_jobQueue;
	std::unordered_map<int, int> m_tidtoHandleMap;
	WorkerThread2* m_workers;

	//���̹��ε��� ť
	//Fiber* m_fiberMagazine;
	//������ ��� �Ű��� �ڵ��� ���
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

		//������ ���ᰡ �ȵ� ���¿��� �̺�Ʈ�� �������� �ȵ�
		//workerThreadLoop()���� ű�̺�Ʈ�� ��ٸ��鼭 �������µ�
		//�̺�Ʈ�� ������ �ٷ� �����ؼ� ������ �����־���
		//while(1)���� ���Ÿ�°� ���� ���� ����ϱ�?
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
			NULL); //�̸����̰� ������

		m_workers[i].hWorkerThread = (HANDLE)_beginthreadex(
			NULL,
			512 * 1024,//stacksize
			workerThreadLoop,
			&m_workers[i],
			0, //CREATE_SUSPENDED?
			&m_workers[i].threadId//thrdaddr; GetThreadId(h) �� ���� �� �ִ°�
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
	��Ʈ���� �׽�Ʈ�� �� �Ǵٰ� �߰��� �����ϰ� ���ߴ� ������ ����
	��ť numObjectsī���Ͱ� ����� �۵��ϰ� ���� �ʴ�
	numObjects�� Ȯ���ؼ� ���� �ϰ� �Ǵµ�
	numObjectsȮ���Ҷ� 1 �̴��� ���Ҷ��� 0���� �Ǵ°� ����.
	�������� SPMC �ΰ� ��������
	Single Produce�ε� ���ݰ��� ��쿣 ���� ���̹����� ���� �ְ� �ִ� ��Ȳ�̴�.
	mpmcť�� ��ü�ؼ� �Ƚ�
	*/

	//if(atomic_load(jq->numObjects))
	{
		//assert(atomic_load(jq->numObjects) > 0);
		//JobDeclaration2 jb = jq->pop();
		JobDeclaration2 jb = {};
		if (jq->dequeue(jb))
		{
			p->pUserdata = jb.pUserdata; //���ο� arg�� ��ü�ϴ°� �� �̻��ϱ��ѵ�. 
			jb.callback(p);
			//���� �Լ� ȣ���� �����Ѵٸ�? ���� �ٽ� ����־���� �ұ�??

			atomic_fetch_sub(jb.pCounter, 1);
			assert(atomic_load(jb.pCounter) != uint32_t(-1));
		}

	}

	//���̹��� �׳� �����ϸ� �ȵǰ�(guard_cleanup2()�Լ��� ȣ���ϰԵ�) ����ġ �� �������
	fiber_switch(p->self, p->caller);
}

unsigned __stdcall workerThreadLoop(LPVOID lpParam)
{
	DWORD ret = 0;
	WorkerThread2* t = (WorkerThread2*)lpParam;
	JobSystem2* sched = t->scheduler;


	//�̷��� �ƴ϶� stop�̺�Ʈ�� ��ٷ��� �Ұ� ����
	while (1)
	{
		ret = WaitForSingleObject(t->ThreadKickEvent, INFINITE);
		
		int who = sched->m_tidtoHandleMap[GetCurrentThreadId()];

		/*
		������ ���̹��� ���µ�-�׷� freeFiberidx = 0 �̵�- 0�� ���̹����� �� �Ǿ �����ϴ� �ٶ��� ������ �־���
		������ ���� �ִ����� Ȯ���� �������� �ʳ�??
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
			//printf("\n ������ ���̹��� ����\n");
			SwitchToThread();
		}
		
		

	}

	
	return 1;
}



void JobSystem2::RunTask(void(*taskJob)(void * args), void* args, std::atomic_uint* counter)
{

	//20200204
	//��? �½�ũ�� �½�ũ�� ������ ���̳� ������ �� �Ȱ��� ���ε� ���� �������̹��� �����ϱ⺸�� �� ��ť�� �־������?
	JobDeclaration2 jobDecls = {};
	jobDecls.callback = taskJob;
	jobDecls.pUserdata = args; //args�� � Ÿ�������� ����Ǵ� �½�ũ�� ���� ������
	//ī���͸� �ۿ��� ���ͺ���.. 
	jobDecls.pCounter = counter;
	if (m_jobQueue->enqueue(jobDecls))
	{
		//���⼭ ű�Ҽ���
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
	//���̹����� ��ٸ��°� �ƴ϶� ��Ŀ�����忡�� ��ٸ��� �Ǵ� ��쿡 ��������
	
	volatile uint32_t c = std::atomic_load(counter);


	/*20210408
	������ ���� ����� �ʿ��ѵ�
	�ʹ� ���� ����ϱ� ���̹��� ���� �Ҹ��ϰԵǴ°� ����
	workerThreadLoop() ��ƾ�� "������ ���̹��� ����" �� �ߴ� ����
	�ѹ��� �����

	�����ٷ��� ������� ��Ŀ�����带 �����״ٰ� �ϰ��� ������ ������ �˰����� �ʿ��ϴ�
	���� ������ ���̹� ������ �ѹ��� �����ϰ� ���°Ŷ� �� ��ǰ�� �츮�� ��������
	���̹� ����Ī�� ������ ���µ� �������̹��� �����ϴºκ��� ������

	https://github.com/simonfxr/fiber
	https://github.com/RichieSams/FiberTaskingLib/blob/master/source/task_scheduler.cpp#L94
	*/
	for (int i = 0; i < m_numWorkerThreads; ++i)
	{
		//runTask()���� ű�ϵ��� �غ���
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
		// �̷��� �ϸ� �ȵɰ� ���� �� ������ 
#if 0
		
		//���� ���̹� ��
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

			//����ġ��
			/*20200210 Ʈ������ freeFiber->regs.sp 
			//���̹� ������ ũ�� ����ִϱ� ���� ��Ƽ�°ɷ� ���� sp�� ��������ߵ�..
			//���ʿ� �츮�� ������ ���̹� �������� �ѹ� ���� ������ �뵵�� �׷��� 
			//Ǯ�� ����� ���� ��Ȱ���ϴµ� �������� ���� ������ �ִ�
			//realloc �� ����� �ʿ���
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

