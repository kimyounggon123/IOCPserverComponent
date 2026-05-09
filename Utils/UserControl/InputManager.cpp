#include "InputManager.h"
InputManager* InputManager::instance = nullptr;

void InputManager::readEveryFrame()
{
    SHORT state;
    for (int key = 0; key < 256; key++) {
        //state = GetAsyncKeyState(key);
        state = GetAsyncKeyState(toupper(key));
        bool pressed = (state & 0x8000) != 0;

        //std::lock_guard<std::mutex> lock(mutex);
        KeyState currKeyState = keysState[key];

        switch (currKeyState)
        {
        case KeyState::Idle:
            if (pressed) keysState[key] = KeyState::Press;
            break;
        case KeyState::Press:
            keysState[key] = pressed ? KeyState::Stay : KeyState::Release;
            break;
        case KeyState::Stay:
            if (!pressed) keysState[key] = KeyState::Release;
            break;
        case KeyState::Release:
            keysState[key] = pressed ? KeyState::Press : KeyState::Idle;
            break;
        default:
            keysState[key] = KeyState::Idle;
            break;
        }
    }
}

bool InputManager::isKeyDown(char key) 
{
    return keysState[key] == KeyState::Press;
}

bool InputManager::isKeyPressed(char key) 
{
    KeyState state = keysState[key];
    return state == KeyState::Press || state == KeyState::Stay;
}

bool InputManager::isKeyReleased(char key)
{
    return keysState[key] == KeyState::Release;
}