#ifndef _INPUTMANAGER_H
#define _INPUTMANAGER_H

#include <Windows.h>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <conio.h>  // _kbhit, _getch
#include <unordered_set>
#include <unordered_map>


enum class KeyState 
{
	Idle,
	Press,
	Stay,
	Release,
};

class InputManager
{

	std::thread inputThread;
	KeyState keysState[256];

	static InputManager* instance;
	InputManager(): keysState{KeyState::Idle}
	{}

public:
	static InputManager& getInstance()
	{
		if (instance == nullptr) instance = new InputManager();
		return *instance;
	}

	void readEveryFrame();

	bool isKeyPressed(char key);
	bool isKeyDown(char key);
	bool isKeyReleased(char key);
};
#endif