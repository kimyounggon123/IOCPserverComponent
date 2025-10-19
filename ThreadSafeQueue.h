#ifndef _THREADSAFEQUEUE_H
#define _THREADSAFEQUEUE_H

#include <stack>
#include <queue>
#include <Windows.h>
#include <atomic>

template <typename T>
class ThreadSafeStack 
{
	std::stack<T> safe_stack;
	CRITICAL_SECTION stack_cs;
	HANDLE stackEvent;
	DWORD howMuchWait;

public:
	ThreadSafeStack(bool hasEvent = false, DWORD howMuchWait = INFINITE): howMuchWait(howMuchWait)
	{
		InitializeCriticalSection(&stack_cs);
		stackEvent = hasEvent ? CreateEvent(NULL, TRUE, FALSE, NULL) : NULL;
	}
	~ThreadSafeStack()
	{
		while (!safe_stack.empty())
		{
			safe_stack.pop();  
		}
		DeleteCriticalSection(&stack_cs);
		if (stackEvent != NULL) CloseHandle(stackEvent);
	}
	ThreadSafeStack(const ThreadSafeStack&) = delete;
	ThreadSafeStack& operator=(const ThreadSafeStack&) = delete;

	bool push(const T& input)
	{
		EnterCriticalSection(&stack_cs);
		safe_stack.push(input);
		if (stackEvent != NULL) SetEvent(stackEvent);
		LeaveCriticalSection(&stack_cs);

		return true;
	}

	// �� �κп��� ��Ŷ�� ���� �� �� pop�� �ϴ� ��찡 ����.
	bool pop(T& output)
	{
		if (stackEvent != NULL) WaitForSingleObject(stackEvent, howMuchWait);
		EnterCriticalSection(&stack_cs);

		if (safe_stack.empty())
		{
			LeaveCriticalSection(&stack_cs);
			return false;
		}

		output = safe_stack.top();
		safe_stack.pop();

		if (stackEvent != NULL && safe_stack.empty())
			ResetEvent(stackEvent); // ������ pop ��
		

		LeaveCriticalSection(&stack_cs);

		return true;
	}

	bool isEmpty()
	{
		EnterCriticalSection(&stack_cs);
		bool empty = safe_stack.empty();
		LeaveCriticalSection(&stack_cs);
		return empty;
	}
};


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
		queueEvent = hasEvent ?  CreateEvent(NULL, TRUE, FALSE, NULL) : NULL;
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
	}

	ThreadSafeQueue(const ThreadSafeQueue&) = delete;
	ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

	bool enqueue(const T& input)
	{
		EnterCriticalSection(&queue_cs);
		safe_queue.push(input);
		if (queueEvent != NULL) SetEvent(queueEvent);
		LeaveCriticalSection(&queue_cs);

		return true;
	}

	bool dequeue(T& output)
	{
		if (queueEvent != NULL)
		{
			DWORD waitResult = WaitForSingleObject(queueEvent, howMuchWait);
			if (waitResult == WAIT_TIMEOUT) return false;
		}

		EnterCriticalSection(&queue_cs);

		if (safe_queue.empty())
		{
			if (queueEvent != NULL) ResetEvent(queueEvent);
			LeaveCriticalSection(&queue_cs);
			return false;
		}

		output = safe_queue.front();
		safe_queue.pop();

		if(queueEvent != NULL && safe_queue.empty())
			ResetEvent(queueEvent); // ������ pop ��
		
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


// Lock free structure
template <typename T>
class LockFreeQueue
{
	struct Node {
		T data; // �ܺο��� ���� �ʿ���
		std::atomic<Node*> next;
		Node(const T& value): data(value), next(nullptr)
		{}
	};

	// ABA ���� ������ ���� pointer + counter ����
	struct TaggedPtr {
		Node* ptr;
		uint64_t tag;
		TaggedPtr(Node* p = nullptr, uint64_t t = 0) : ptr(p), tag(t)
		{
		}
	};

	std::atomic<TaggedPtr> head; // �ֻ�� Node
	std::atomic<TaggedPtr> tail; // �ֻ�� Node
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
	// param0 == param1�� ��� param0�� param2�� ���� �� return true;
	// param0 != param1�� ��� param1�� param0���� ���� �� return false;

	void enqueue(const T& value)
	{
		Node* new_node = new Node(value);
		Node* curr_tail;

		while (true)
		{
			curr_tail = tail.load(); // ���� tail�� load
			Node* next = curr_tail->next.load(); // �� tail�� ���� �κ��� load

			if (curr_tail == tail)
			{
				if (next == nullptr) // tail ��尡 ���� ������ ��������� Ȯ����
				{
					// new_node�� tail->next�� ���� �õ�
					if (curr_tail->next.compare_exchange_weak(next, new_node)) break; // ���� �� ���� Ż��
				}
				// tail�� ���� ���� �̵� �õ�
				else tail.compare_exchange_weak(curr_tail, next);
			}
		}

		// tail �����͸� �� ���� �ֽ�ȭ
		tail.compare_exchange_weak(curr_tail, new_node);
	}

	bool dequeue(T& output)
	{
		Node* curr_head;
		while (true)
		{
			curr_head = head.load(); // �� head�� load (dummy)
			Node* next = curr_head->next.load(); // head ���� node�� �ҷ� ��

			if (next == nullptr) return false; // �� queue��� return false
			if (head.compare_exchange_weak(curr_head, next)) // �� head == curr_head��� head flag�� next�� �̵� (������ next�� head�� �Ǿ� dummy head�� ��)
			{
				// next�� �����͸� �̸� ���� ��
				output = next->data;
				delete curr_head; // ���� ����� ����
				break;
			}
		}
		return true;
	}

	bool isEmpty()
	{
		Node* next = head.load()->next.load(); // head ���� node�� �ҷ� ��
		if (next != nullptr) return false; 
		return true; // �� queue��� return true
	}

};


template <typename T>
class LockFreeStack
{
	struct Node {
		T data; // �ܺο��� ���� �ʿ���
		std::atomic<Node*> next;
		Node(const T& value) : data(value), next(nullptr)
		{}
	};

	// ABA ���� ������ ���� pointer + counter ����
	// ABA: ��Ƽ�����忡�� atomic compare-and-swap(CAS)�� ����� �� ����� ���� ����.
	struct TaggedPtr {
		Node* ptr;
		uint64_t tag;
		TaggedPtr(Node* p = nullptr, uint64_t t = 0) : ptr(p), tag(t)
		{}
	};

	std::atomic<TaggedPtr> head; // �ֻ�� Node

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
	// param0 == param1�� ��� param0�� param2�� ���� �� return true;
	// param0 != param1�� ��� param1�� param0���� ���� �� return false;
	// atomic ��ü ��ü�� ���ϴ� �Լ��̴�. ������ ��� field�� ���� ���̿��߸� return true

	void push(const T& value)
    {
        Node* new_node = new Node(value); // �� ���
        TaggedPtr old_head; // ���� head

        while (true) {
            old_head = head.load(); // ���� head load
            new_node->next = old_head.ptr; // �� head ���� 
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
				delete node; // ���⼭ ���� �߻�
				return true;
			}
		}

		return false;
	}

	bool isEmpty() const {	return head.load().ptr == nullptr; }
};
#endif