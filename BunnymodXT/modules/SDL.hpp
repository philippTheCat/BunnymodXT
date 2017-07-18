#pragma once

#include "../sptlib-wrapper.hpp"
#include <SPTLib/IHookableNameFilter.hpp>
#include "../hud_custom.hpp"
#include "SDL.h"

class SDL : public IHookableNameFilter
{
public:
	static SDL& GetInstance()
	{
		static SDL instance;
		return instance;
	}

	virtual void Hook(const std::wstring& moduleName, void* moduleHandle, void* moduleBase, size_t moduleLength, bool needToIntercept);
	virtual void Unhook();
	virtual void Clear();

	void PressKey(SDL_Keycode key);

private:
	SDL() : IHookableNameFilter({ L"SDL2.dll", L"libSDL2.so" }) {};
	SDL(const SDL&);
	void operator=(const SDL&);

protected:
	typedef int(__cdecl *_SDL_PushEvent) (SDL_Event*);
	_SDL_PushEvent ORIG_SDL_PushEvent;
	typedef SDL_Scancode(__cdecl *_SDL_GetScancodeFromKey) (SDL_Keycode);
	_SDL_GetScancodeFromKey ORIG_SDL_GetScancodeFromKey;

	void FindStuff();
};
