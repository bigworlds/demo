#pragma once

#include <Windows.h>
#include <malloc.h>
#include <process.h>
#include <assert.h>
#include <stdint.h>

//���̹� 64��Ʈ�� �ٽ��غ��߰ڴ�
//https://github.com/simonfxr/fiber/
//�����ٷ����� ���⺸�� �ܼ��� ������ ���� ���°� ������ ����.

#define FIBER_TARGET_AMD64_WIN64 1
#define FIBER_CCONV

#define FIBER_FS_EXECUTING 1
#define FIBER_FS_TOPLEVEL 2
#define FIBER_FS_ALIVE 4
#define FIBER_FS_HAS_LO_GUARD_PAGE 8
#define FIBER_FS_HAS_HI_GUARD_PAGE 16
#define FIBER_FLAG_GUARD_LO 8
#define FIBER_FLAG_GUARD_HI 16


#define STACK_ALIGNMENT ((uintptr_t) 16)
static const size_t WORD_SIZE = sizeof(void *); //you mean QWORD?
#define IS_STACK_ALIGNED(x) (((uintptr_t)(x) & (STACK_ALIGNMENT - 1)) == 0)
#define STACK_ALIGN(x) ((uintptr_t)(x) & ~(STACK_ALIGNMENT - 1))
//dwPageSize; 4KiB
static const size_t PAGE_SIZE = 4096;


struct FiberRegs //_CONTEXT
{
	void *sp;
	void *lr; //Link Register? �̰� arm ���� ���°� ������?
	void *rbx;
	void *rbp;
	void *rdi;
	void *rsi;
	void *r12;
	void *r13;
	void *r14;
	void *r15;

	/*To make 16byte alignment possible */
	double xmm[20 + 1]; //160 + 8 Bytes

};

struct Fiber
{
	FiberRegs regs; 
	void* stack;
	void* alloc_stack;
	int stack_size;
	uint32_t state;
};
typedef void(FIBER_CCONV *FiberFunc)(void*);
//using FiberFunc = void( *)(void* );
typedef void(FIBER_CCONV *FiberCleanupFunc)(Fiber*, void*);



extern "C"
{
	void fiber_asm_switch(FiberRegs* from, FiberRegs* to);
	// _invoke() ���ÿ� Ǫ���ؾ��ϴ°�: old sp, function pointer, args
	void fiber_asm_invoke();
	void fiber_asm_exec_on_stack(void*, FiberFunc, void*);

	void fiber_align_check_failed()
	{
#ifndef NDEBUG
		assert(0 && "ERROR: fiber stack alignment check failed");
#else
		fprintf(stderr, "ERROR: fiber stack alignment check failed\n");
#endif
		abort();
	}
}









//�̸� ��Ƶ� �������� ���̹� �ʱ�ȭ
//Ŭ���� �Լ��� ��? �ٸ� ���̹��� �����ϴµ� ����Ѵ���. -> ž���� ���̹���. ���ʷα׶���ұ�.
Fiber* fiber_init(Fiber* fiber, void* stack, size_t stack_size, FiberCleanupFunc cleanup, void* arg);

//ž���� ���̹��� ������ ��������� ����°� ����ϴµ�
//������Ǯ ���鶧 ȣ���ϸ�ɵ�
void fiber_init_toplevel(Fiber* fiber);

//���ñ��� ���� ���� ����� ���
//bottom/top ������������ ���Ҹ���
//fiber_reserve_return() ���� ������������(4KB)��ŭ �����ϴ°� ������
//�������� caller�� �ϴ½��ε�
bool fiber_alloc(Fiber* fiber, size_t stack_size, FiberCleanupFunc, void* arg, uint32_t flags);

void fiber_destroy(Fiber* fiber);

//context-switch
void fiber_switch(Fiber* from, Fiber* to);

//to ���̹��� ����ġ�ϱ����� �����ּҸ� Ǫ��-prologue
void fiber_reserve_return(Fiber* fiber, FiberFunc f, void** args_dest, size_t args_size);
//void fiber_push_return(Fiber* fiber, FiberFunc f, const void* args, size_t s);

//in-place?
//void fiber_exec_on(Fiber* active, Fiber* temp, FiberFunc f, void* args);

static inline bool fiber_is_toplevel(Fiber *fiber)
{
	return (fiber->state & FIBER_FS_TOPLEVEL) != 0;
}

static inline bool fiber_is_executing(Fiber* fiber)
{
	return (fiber->state & FIBER_FS_EXECUTING) != 0;
}

static inline bool fiber_is_alive(Fiber *fiber)
{
	return (fiber->state & FIBER_FS_ALIVE) != 0;
}

static inline void fiber_set_alive(Fiber *fiber, bool alive)
{
	if (alive)
	{
		fiber->state |= FIBER_FS_ALIVE;
	}
	else
	{
		fiber->state &= ~FIBER_FS_ALIVE;
	}
}

static inline void* fiber_stack(const Fiber *fiber)
{
	return fiber->stack;
}

static inline size_t fiber_stack_size(const Fiber *fiber)
{
	return fiber->stack_size;
}

static inline size_t fiber_stack_free_size(const Fiber *fiber)
{
	return fiber->stack_size - (static_cast<char *>(fiber->regs.sp) -
		static_cast<char *>(fiber->stack));
}





struct FiberGuardArgs
{
	Fiber *fiber;
	FiberCleanupFunc cleanup;
	void *arg;
};

static void fiber_guard(void *arg)
{
	FiberGuardArgs* grd = (FiberGuardArgs *)arg;
	grd->fiber->state &= ~FIBER_FS_ALIVE;
	grd->cleanup(grd->fiber, grd->arg);
	assert(0);//Ŭ������ �����ϸ� �ȵ�...
}

void _fiber_init(Fiber* fiber, FiberCleanupFunc cleanup, void* arg)
{
	memset(&fiber->regs, 0, sizeof(FiberRegs));
	uintptr_t sp = (uintptr_t)((char *)fiber->stack + fiber->stack_size - WORD_SIZE);
	sp &= ~(STACK_ALIGNMENT - 1);
	fiber->regs.sp = (void *)sp;
	FiberGuardArgs* args;
	fiber_reserve_return(fiber, fiber_guard, (void **)&args, sizeof(*args));
	args->fiber = fiber;
	args->cleanup = cleanup;
	args->arg = arg;
	fiber->state |= FIBER_FS_ALIVE;
}

Fiber* fiber_init(Fiber* fiber, void* stack, size_t stack_size, FiberCleanupFunc cleanup, void* arg)
{
	fiber->stack = stack;
	fiber->stack_size = stack_size;
	fiber->alloc_stack = nullptr;
	fiber->state = 0;

	_fiber_init(fiber, cleanup, arg);


	return fiber;

}

static void* alloc_pages(size_t npages)
{
	void* ret = _aligned_malloc(npages*PAGE_SIZE, PAGE_SIZE);
	return ret;
}

static void free_pages(void* p)
{
	_aligned_free(p);
}

static bool protect_page(void* p, bool rw)
{
	DWORD old_protect;
	return VirtualProtect(p, PAGE_SIZE, rw ? PAGE_READWRITE : PAGE_NOACCESS, &old_protect) != 0;
}

bool fiber_alloc(Fiber* fiber, size_t size, FiberCleanupFunc cleanup, void* arg, uint32_t flags)
{
	flags &= FIBER_FLAG_GUARD_LO | FIBER_FLAG_GUARD_HI;
	fiber->stack_size = size;
	const size_t stack_size = size;
	if (!flags)
	{
		fiber->alloc_stack = fiber->stack = malloc(stack_size); //_aligned_malloc
		if (!fiber->alloc_stack)
		{
			return false;
		}
	}
	else
	{
		size_t npages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
		if (flags & FIBER_FLAG_GUARD_LO)
		{
			++npages;
		}
		if (flags & FIBER_FLAG_GUARD_HI)
		{
			++npages;
		}
		fiber->alloc_stack = alloc_pages(npages);
		if (!fiber->alloc_stack)
		{
			return false;
		}
		if (flags & FIBER_FLAG_GUARD_LO)
		{
			if (!protect_page((char *)fiber->alloc_stack, false))
			{
				goto fail; //goto �̰� ���Ե����̾�
			}
		}
		if (flags & FIBER_FLAG_GUARD_HI)
		{
			if (!protect_page((char *)fiber->alloc_stack + (npages - 1) * PAGE_SIZE, false))
			{
				goto fail;
			}
		}
		if (flags & FIBER_FLAG_GUARD_LO)
		{
			fiber->stack = (char *)fiber->alloc_stack + PAGE_SIZE;
		}
		else
		{
			fiber->stack = fiber->alloc_stack;
		}

	}

	fiber->state = flags;
	_fiber_init(fiber, cleanup, arg);
	return true;

fail:
	free_pages(fiber->alloc_stack);
	return false;
}

void fiber_destroy(Fiber* fiber)
{
	if (!fiber->alloc_stack)
	{
		return;
	}

	if (fiber->state & (FIBER_FS_HAS_HI_GUARD_PAGE | FIBER_FS_HAS_LO_GUARD_PAGE))
	{
		size_t npages = (fiber->stack_size + PAGE_SIZE - 1) / PAGE_SIZE; //ceil()�����ǰ�
		if (fiber->state & FIBER_FS_HAS_LO_GUARD_PAGE)
		{
			++npages;
			protect_page(fiber->alloc_stack, true);
		}
		if (fiber->state & FIBER_FS_HAS_HI_GUARD_PAGE)
		{
			protect_page((char*)fiber->alloc_stack + npages * PAGE_SIZE, true);
		}

		free_pages(fiber->alloc_stack);
	}
	else
	{
		free(fiber->alloc_stack);
	}

	fiber->stack = nullptr;
	fiber->stack_size = 0;
	fiber->regs.sp = nullptr;
	fiber->alloc_stack = nullptr;
}

void fiber_switch(Fiber* from, Fiber* to)
{
	assert(from);
	assert(to);

	if (from == to)
	{
		return;
	}

	assert(fiber_is_executing(from));
	assert(!fiber_is_executing(to));

	from->state &= ~FIBER_FS_EXECUTING; //disable
	to->state |= FIBER_FS_EXECUTING; //enable
	fiber_asm_switch(&from->regs, &to->regs);
}

void fiber_reserve_return(Fiber* fiber, FiberFunc f, void** args, size_t s)
{

	assert(!fiber_is_executing(fiber));

	char* sp = (char *)fiber->regs.sp;
	sp = (char *)STACK_ALIGN(sp);
	s = (s + STACK_ALIGNMENT - 1) & ~((size_t)STACK_ALIGNMENT - 1);
	assert(IS_STACK_ALIGNED(sp) && "1");
	*args = sp;

	//push gpr
	sp -= s;
	*args = (void *)sp;
	assert(IS_STACK_ALIGNED(sp) && "2");

	sp -= sizeof(fiber->regs.sp);
	*(void **)sp = fiber->regs.sp;

	sp -= sizeof *args;
	*(void **)sp = *args;

	//push 
	sp -= sizeof(FiberFunc);
	*(FiberFunc *)sp = f;

	sp -= WORD_SIZE; // introduced to realign stack to 16 bytes

	assert(IS_STACK_ALIGNED(sp) && "3");

	//push
	sp -= sizeof(FiberFunc);
	*(FiberFunc *)sp = (FiberFunc)fiber_asm_invoke;

	//FIBER_TARGET_64_AARCH ?

	fiber->regs.sp = (void *)sp;
}

//static inline char* stack_align_n(char* sp, size_t n)
//{
	//return (char *)((uintptr_t) sp & ~(uintptr_t)(n-1));
//}

//bool is_stack_aligned(void* sp)
//{
	//return ((uintptr_t) sp & (FIBER_DEFAULT_STACK_ALIGNMENT - 1)) == 0;
//}

void fiber_init_toplevel(Fiber* fiber)
{
	fiber->stack = nullptr;
	fiber->stack_size = (size_t)-1;
	fiber->alloc_stack = nullptr;
	memset(&fiber->regs, 0, sizeof(fiber->regs));
	fiber->state = FIBER_FS_ALIVE | FIBER_FS_TOPLEVEL | FIBER_FS_EXECUTING;
}







//static void probe_stack(volatile char* sp0, size_t sz, size_t pgsz)
//{
	//volatile char* sp = sp0;
	//size_t i = 0;
	//while(i < sz)
	//{
		//*(volatile uintptr_t *)sp |= (uintptr_t)0;
		//i += pgsz;
		//sp -= pgsz;
	//}

	//HAVE_probe_stack_weak_dummy ?? �𸣴°� �ʹ� ������
//}

//static inline void push(char** sp, void* val)
//{
	//*sp -= WORD_SIZE;
	//*(void **)*sp = val;
//}

void fiber_push_return(Fiber* fiber, FiberFunc f, const void* args, size_t s)
{
	void* args_dest;
	fiber_reserve_return(fiber, f, &args_dest, s);
	memcpy(args_dest, args, s);
}

void fiber_exec_on(Fiber* active, Fiber* temp, FiberFunc f, void* args)
{
	assert(fiber_is_executing(active));

	if (active == temp)
	{
		f(args);
	}
	else
	{
		assert(!fiber_is_executing(temp));
		temp->state |= FIBER_FS_EXECUTING;
		active->state &= ~FIBER_FS_EXECUTING;
		fiber_asm_exec_on_stack(args, f, temp->regs.sp);
		active->state |= FIBER_FS_EXECUTING;
		temp->state &= ~FIBER_FS_EXECUTING;
	}
}


