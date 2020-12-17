#pragma once
#include "OpenFunscripterUtil.h"
#include "SDL.h"

#include <vector>
#include <functional>

// insanely basic event system
// it doesn't get any simpler than this
using EventHandlerFunc = std::function<void(SDL_Event&)>;

class EventHandler {
public:
	int32_t eventType;
	EventHandlerFunc func;
	void* listener = nullptr;

	EventHandler(int32_t type, void* listener, EventHandlerFunc func)
		: eventType(type), func(func), listener(listener) { }
};

class EventSystem {
private:
	std::vector<EventHandler> handlers;

	void SingleShotHandler(SDL_Event& ev) noexcept;
public:
	// custom events
	static int32_t FunscriptActionsChangedEvent;
	static int32_t FunscriptActionClickedEvent;
	static int32_t FunscriptSelectionChangedEvent;

	static int32_t WakeupOnMpvEvents;
	static int32_t WakeupOnMpvRenderUpdate;

	static int32_t FfmpegAudioProcessingFinished;

	static int32_t MpvVideoLoaded;

	static int32_t ControllerButtonRepeat;

	using SingleShotEventHandler = std::function<void(void*)>;
	struct SingleShotEventData {
		void* ctx = nullptr;
		SingleShotEventHandler handler;
	};
	// this event + SingleShotEventData + SingleShotHandler allows execute arbitrary code
	// back on the main thread comming from another. currently this is used to return filepaths from file dialogs.
	static int32_t SingleShotEvent;

	void setup();

	void PushEvent(SDL_Event& event) noexcept;
	void Subscribe(int32_t eventType, void* listener, EventHandlerFunc&& handler) noexcept;
	void Unsubscribe(int32_t eventType, void* listener) noexcept;
	void UnsubscribeAll(void* listener) noexcept;

	static void SingleShot(SingleShotEventHandler&& handler, void* ctx) noexcept;
};

#define EVENT_SYSTEM_BIND(listener, handler) listener, std::move(std::bind(handler, listener, std::placeholders::_1))