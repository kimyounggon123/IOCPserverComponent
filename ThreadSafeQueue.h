#ifndef _THREADSAFEQUEUE_H
#define _THREADSAFEQUEUE_H

#include <stack>
#include <queue>
#include <Windows.h>
#include <atomic>
#include <iostream>
#include <chrono>
#include <mutex>
#include <condition_variable>

template <typename T>
class ThreadSafeStack
{
	std::stack<T> safe_stack;
	std::mutex stack_mtx;
	std::condition_variable stack_cv;
	DWORD timeout_ms;

public:

	ThreadSafeStack(DWORD timeout = INFINITE) : timeout_ms(timeout) {}
	~ThreadSafeStack() = default;

	// 복사 금지
	ThreadSafeStack(const ThreadSafeStack&) = delete;
	ThreadSafeStack& operator=(const ThreadSafeStack&) = delete;

	bool push(T input) {
		
		std::lock_guard<std::mutex> lock(stack_mtx);
		safe_stack.push(std::move(input));
		stack_cv.notify_one();
		return true;
	}

	bool pop(T& output) {
		std::unique_lock<std::mutex> lock(stack_mtx);
		if (timeout_ms == INFINITE)
		{
			stack_cv.wait(lock, [this] { return !safe_stack.empty(); });
		}
		else {
			if (!stack_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] { return !safe_stack.empty(); }))
			{
				return false; // 타임아웃
			}
		}

		output = std::move(safe_stack.top());
		safe_stack.pop();
		return true;

	}

	bool pop_nowait(T& output)
	{
		output = std::move(safe_stack.top());
		safe_stack.pop();
		return true;
	}


	bool isEmpty() {
		std::lock_guard<std::mutex> lock(stack_mtx);
		return safe_stack.empty();
	}

	size_t size() {
		std::lock_guard<std::mutex> lock(stack_mtx);
		return safe_stack.size();
	}

	void setTimems(DWORD timems)
	{
		timeout_ms = timems;
	}

};

template<typename T>
class ThreadSafeQueue {
	std::queue<T> safe_queue;
	std::mutex queue_mtx;
	std::condition_variable queue_cv;
	DWORD timeout_ms;

public:
	ThreadSafeQueue(DWORD timeout_ms = INFINITE) : timeout_ms(timeout_ms)
	{}
	~ThreadSafeQueue() = default;

	ThreadSafeQueue(const ThreadSafeQueue&) = delete;
	ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

	bool enqueue(T input) {
		std::lock_guard<std::mutex> lock(queue_mtx);
		safe_queue.push(std::move(input));
		queue_cv.notify_one();
		return true;
	}

	bool dequeue(T& output) {
		std::unique_lock<std::mutex> lock(queue_mtx);

		if (timeout_ms == INFINITE)
			queue_cv.wait(lock, [this] { return !safe_queue.empty(); });

		else if (!queue_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
			[this] { return !safe_queue.empty(); }))
			return false;

		output = std::move(safe_queue.front());   // move
		safe_queue.pop();
		return true;
	}

	bool dequeue_nowait(T& output)
	{
		output = std::move(safe_queue.front());   // move
		safe_queue.pop();
		return true;
	}

	bool isEmpty()
	{
		std::lock_guard<std::mutex> lock(queue_mtx);
		return safe_queue.empty();
	}
	size_t size()
	{
		std::lock_guard<std::mutex> lock(queue_mtx);
		return safe_queue.size();
	}

	void setTimems(DWORD timems)
	{
		timeout_ms = timems;
	}
};




/*
template <typename T>
class ThreadSafeQueue
{
	std::queue<T> safe_queue;
	CRITICAL_SECTION queue_cs;
	HANDLE queueEvent;
	DWORD howMuchWait;
public:
	ThreadSafeQueue(bool hasEvent = false, DWORD howMuchWait = INFINITE) : howMuchWait(howMuchWait)
	{
		InitializeCriticalSection(&queue_cs);
		queueEvent = hasEvent ?  CreateEvent(NULL, FALSE, FALSE, NULL) : NULL;
	}
	~ThreadSafeQueue()
	{

		while (!safe_queue.empty())
		{
			T object = safe_queue.front();
			safe_queue.pop();
		}
		DeleteCriticalSection(&queue_cs);
		if (queueEvent != NULL) CloseHandle(queueEvent);
		printf("ThreadSafeQueue destroyed\n");
	}

	ThreadSafeQueue(const ThreadSafeQueue&) = delete;
	ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

	
	bool enqueue(const T& input)
	{
		if (queue_cs.LockCount < -1 || queue_cs.SpinCount == 0xFFFFFFFF) 
		{
			printf("Deadlock detected in ThreadSafeQueue::enqueue\n");
			return false;
		}
		EnterCriticalSection(&queue_cs);
		safe_queue.push(input);
		LeaveCriticalSection(&queue_cs);

		if (queueEvent != NULL) SetEvent(queueEvent);
		return true;
	}

	bool dequeue(T& output)
	{
		if (queueEvent != NULL)
		{
			DWORD waitResult = WaitForSingleObject(queueEvent, howMuchWait);
			if (waitResult != WAIT_OBJECT_0) return false;
		}

		EnterCriticalSection(&queue_cs);

		if (safe_queue.empty())
		{
			if (queueEvent != NULL) ResetEvent(queueEvent);
			LeaveCriticalSection(&queue_cs);
			return false;
		}

		auto& val = safe_queue.front();
		output = val;

		//output = std::move(safe_queue.front());
		safe_queue.pop();


		if (queueEvent != NULL && !safe_queue.empty())
			SetEvent(queueEvent); // 마지막 pop 후 manual lock

		LeaveCriticalSection(&queue_cs);

		return true;
	}

	bool isEmpty()
	{
		EnterCriticalSection(&queue_cs);
		bool empty = safe_queue.empty();
		LeaveCriticalSection(&queue_cs);
		return empty;
	}
};
*/

// Lock free structure
template <typename T>
class LockFreeQueue
{
	struct Node {
		T data; // 외부에서 삭제 필요함
		std::atomic<Node*> next;
		Node(const T& value): data(value), next(nullptr)
		{}
	};

	// ABA 문제 방지를 위한 pointer + counter 구조
	struct TaggedPtr {
		Node* ptr;
		uint64_t tag;
		TaggedPtr(Node* p = nullptr, uint64_t t = 0) : ptr(p), tag(t)
		{
		}
	};

	std::atomic<TaggedPtr> head; // 최상단 Node
	std::atomic<TaggedPtr> tail; // 최상단 Node
public:
	LockFreeQueue(): head(TaggedPtr(nullptr, 0)), tail(TaggedPtr(nullptr, 0))
	{}

	~LockFreeQueue()
	{
		Node* curr = head.load();
		while (curr != nullptr)
		{
			Node* next = curr->next.load();
			delete curr;
			curr = next;
		}
	}

	// param0.compare_exchange_weak(param1, param2) 
	// param0 == param1일 경우 param0을 param2로 변경 후 return true;
	// param0 != param1일 경우 param1을 param0으로 변경 후 return false;

	void enqueue(const T& value)
	{
		Node* new_node = new Node(value);
		Node* curr_tail;

		while (true)
		{
			curr_tail = tail.load(); // 현재 tail을 load
			Node* next = curr_tail->next.load(); // 현 tail의 다음 부분을 load

			if (curr_tail == tail)
			{
				if (next == nullptr) // tail 노드가 실제 마지막 노드인지를 확인함
				{
					// new_node를 tail->next에 연결 시도
					if (curr_tail->next.compare_exchange_weak(next, new_node)) break; // 성공 시 루프 탈출
				}
				// tail을 다음 노드로 이동 시도
				else tail.compare_exchange_weak(curr_tail, next);
			}
		}

		// tail 포인터를 새 노드로 최신화
		tail.compare_exchange_weak(curr_tail, new_node);
	}

	bool dequeue(T& output)
	{
		Node* curr_head;
		while (true)
		{
			curr_head = head.load(); // 현 head를 load (dummy)
			Node* next = curr_head->next.load(); // head 다음 node를 불러 옴

			if (next == nullptr) return false; // 빈 queue라면 return false
			if (head.compare_exchange_weak(curr_head, next)) // 현 head == curr_head라면 head flag를 next로 이동 (기존의 next가 head가 되어 dummy head가 됨)
			{
				// next의 데이터를 미리 가져 옴
				output = next->data;
				delete curr_head; // 현재 헤더를 삭제
				break;
			}
		}
		return true;
	}

	bool isEmpty()
	{
		Node* next = head.load()->next.load(); // head 다음 node를 불러 옴
		if (next != nullptr) return false; 
		return true; // 빈 queue라면 return true
	}

};


template <typename T>
class LockFreeStack
{
	struct Node {
		T data; // 외부에서 삭제 필요함
		std::atomic<Node*> next;
		Node(const T& value) : data(value), next(nullptr)
		{}
	};

	// ABA 문제 방지를 위한 pointer + counter 구조
	// ABA: 멀티스레드에서 atomic compare-and-swap(CAS)을 사용할 때 생기는 논리적 오류.
	struct TaggedPtr {
		Node* ptr;
		uint64_t tag;
		TaggedPtr(Node* p = nullptr, uint64_t t = 0) : ptr(p), tag(t)
		{}
	};

	std::atomic<TaggedPtr> head; // 최상단 Node

public:
	LockFreeStack():
		head(TaggedPtr(nullptr, 0)) 
	{}
	~LockFreeStack()
	{
		Node* node = head.load().ptr;
		while (node) {
			Node* next = node->next;
			if (node) delete node;
			node = next;
		}
	}

	// param0.compare_exchange_weak(param1, param2) 
	// param0 == param1일 경우 param0을 param2로 변경 후 return true;
	// param0 != param1일 경우 param1을 param0으로 변경 후 return false;
	// atomic 전체 객체를 비교하는 함수이다. 때문에 모든 field가 같은 값이여야만 return true

	void push(const T& value)
    {
        Node* new_node = new Node(value); // 새 노드
        TaggedPtr old_head; // 기존 head

        while (true) {
            old_head = head.load(); // 기존 head load
            new_node->next = old_head.ptr; // 새 head 넣음 
            TaggedPtr new_head(new_node, old_head.tag + 1); 

            if (head.compare_exchange_weak(old_head, new_head)) 
                break;
        }
    }

	bool pop(T& output)
	{
		TaggedPtr old_head;

		while (true) 
		{
			old_head = head.load();
			Node* node = old_head.ptr;
			if (!node) return false;

			TaggedPtr new_head(node->next, old_head.tag + 1);

			if (head.compare_exchange_weak(old_head, new_head)) 
			{
				output = node->data;
				delete node; // 여기서 문제 발생
				return true;
			}
		}

		return false;
	}

	bool isEmpty() const {	return head.load().ptr == nullptr; }
};
#endif