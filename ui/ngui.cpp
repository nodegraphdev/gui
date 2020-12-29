#include <ngui.h>
#include <iostream>

namespace ng::ui
{

SDL_Rect Box::toSDLRect()
{
	return SDL_Rect{x, y, w, h};
}

Application::Application()
{
	SDL_Init(SDL_INIT_VIDEO);
	IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);
}

Application::~Application()
{
	IMG_Quit();
	SDL_Quit();
}

void Application::addWindow(Window *win)
{
	windows.push_back(win);
}

void Application::mainLoop()
{
	while (true)
	{
		for (auto *win : windows)
		{
			win->update();
		}
		// For debugging
		SDL_Delay(100);
	}
}

Window::Window(const char *name)
{
	window = SDL_CreateWindow(name, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 720, 720,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	renderer = new Renderer(this);
}

Window::~Window()
{
	if (central)
		delete central;
	if (window)
		SDL_DestroyWindow(window);
	if (renderer)
		delete renderer;
}

Renderer::Renderer(Window *win)
{
	window = win;
	renderer = SDL_CreateRenderer(win->window, -1, SDL_RENDERER_ACCELERATED);
}

Renderer::~Renderer()
{
	SDL_DestroyRenderer(renderer);
}

void Renderer::clear()
{
	SDL_RenderClear(renderer);
}

void Renderer::present()
{
	SDL_RenderPresent(renderer);
}

void Renderer::rect(Box at, Color color)
{
	SDL_Rect rect = at.toSDLRect();

	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.g, color.a);
	SDL_RenderDrawRect(renderer, &rect);
}

Texture Renderer::loadImage(const char *path)
{
	SDL_Surface *surface = IMG_Load(path);
	if (!surface)
		throw std::runtime_error("Could not load image from path");

	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
	if (!texture)
	{
		if (surface)
			SDL_FreeSurface(surface);

		throw std::runtime_error("Could not convert surface to texture");
	}

	return Texture(texture);
}

void Renderer::texture(Texture texture, Box at)
{
	SDL_Rect dest = at.toSDLRect();

	SDL_RenderCopy(renderer, texture.texture.get(), nullptr, &dest);
}

void Window::update()
{
	renderer->clear();

	if (central)
	{
		central->render(Box(getSize()), *renderer);
	}

	renderer->present();
}

Size Window::getSize()
{
	Size size;
	SDL_GetWindowSize(window, &size.w, &size.h);
	return size;
}

void Widget::render(Box boundingBox, Renderer &renderer)
{
	renderer.rect(boundingBox, Color(255, 255, 255));

	for (auto c : children)
		c->render(boundingBox, renderer);
}

} // ng::ui
