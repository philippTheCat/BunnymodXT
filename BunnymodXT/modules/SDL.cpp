#include "../stdafx.hpp"

#include "../sptlib-wrapper.hpp"
#include <SPTLib/MemUtils.hpp>
#include <SPTLib/Hooks.hpp>
#include "SDL.hpp"

void SDL::Hook(const std::wstring& moduleName, void* moduleHandle, void* moduleBase, size_t moduleLength, bool needToIntercept)
{
	Clear(); // Just in case.

	m_Handle = moduleHandle;
	m_Base = moduleBase;
	m_Length = moduleLength;
	m_Name = moduleName;
	m_Intercepted = needToIntercept;

	FindStuff();
}

void SDL::Unhook()
{
}

void SDL::Clear()
{
	IHookableNameFilter::Clear();
	ORIG_SDL_PushEvent = nullptr;
	ORIG_SDL_GetScancodeFromKey = nullptr;
}

void SDL::FindStuff()
{
	ORIG_SDL_PushEvent = reinterpret_cast<_SDL_PushEvent>(MemUtils::GetSymbolAddress(m_Handle, "SDL_PushEvent"));
	if (ORIG_SDL_PushEvent)
	{
		EngineDevMsg("[SDL] Found SDL_PushEvent() at %p.\n", ORIG_SDL_PushEvent);
	}
	else
	{
		EngineDevWarning("[SDL] Couldn't get the address of SDL_PushEvent().\n");
		EngineWarning("bxt_press_key is not available.\n");
	}

	ORIG_SDL_GetScancodeFromKey = reinterpret_cast<_SDL_GetScancodeFromKey>(MemUtils::GetSymbolAddress(m_Handle, "SDL_GetScancodeFromKey"));
	if (ORIG_SDL_GetScancodeFromKey)
	{
		EngineDevMsg("[SDL] Found SDL_GetScancodeFromKey() at %p.\n", ORIG_SDL_GetScancodeFromKey);
	}
	else
	{
		ORIG_SDL_PushEvent = nullptr;
		EngineDevWarning("[SDL] Couldn't get the address of SDL_GetScancodeFromKey().\n");
		EngineWarning("bxt_press_key is not available.\n");
	}
}

void SDL::PressKey(SDL_Keycode key)
{
	if (!ORIG_SDL_PushEvent)
		return;

	SDL_Event event;
	event.type = SDL_KEYDOWN;
	event.key.state = SDL_PRESSED;
	event.key.repeat = 0;
	event.key.keysym.scancode = ORIG_SDL_GetScancodeFromKey(key);
	event.key.keysym.sym = key;
	ORIG_SDL_PushEvent(&event);

	event.type = SDL_KEYUP;
	event.key.state = SDL_RELEASED;
	ORIG_SDL_PushEvent(&event);
}
