#include <ngui.h>

namespace ng::ui
{

Application::Application()
{}

void Application::mainLoop()
{
	while (true)
	{}
}

Window::~Window()
{
	if (central)
		delete central;
}

Renderer::Renderer()
{}

void Window::update()
{}

void Widget::render(Box &boundingBox, Renderer &renderer)
{}

}