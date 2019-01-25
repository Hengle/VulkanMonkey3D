#include "Code/Window/Window.h"
#include "Code/Timer/Timer.h"
#include <iostream>
#include <cstring>
#include <string>

using namespace vm;

int main(int argc, char* argv[])
{

	Window::create();

	while(true)
	{
		Timer timer;
		timer.minFrameTime(1.f / (float)GUI::fps);

		if (!Window::processEvents(timer.getDelta(GUI::timeScale)))
			break;

		for (auto& renderer : Window::renderer) {
			renderer->update(timer.getDelta(GUI::timeScale));
			renderer->present();
		}
		if (timer.intervalsOf(0.25f)) {
			GUI::cpuWaitingTime = Window::renderer[0]->waitingTime * 1000.f;
			GUI::cpuTime = Timer::noWaitDelta * 1000.f;
			GUI::gpuTime = Window::renderer[0]->ctx.metrics.getGPUFrameTime();
		}
	}

	Window::destroyAll();
	Script::Cleanup();

	return 0;
}