#pragma once

#include <list>
#include <memory>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

namespace ng::ui
{

struct Point
{
	int x, y;
};

struct Size
{
	int w, h;
};

struct Box
{
	Box(Size size)
		: x(0)
		, y(0)
		, w(size.w)
		, h(size.h)
	{}

	Box(Point point, Size size)
		: x(point.x)
		, y(point.y)
		, w(size.w)
		, h(size.h)
	{}

	int x, y, w, h;

	SDL_Rect toSDLRect();
};

struct Color
{
	Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
		: r(r)
		, g(g)
		, b(b)
		, a(a)
	{}

	Color(unsigned char r, unsigned char g, unsigned char b)
		: r(r)
		, g(g)
		, b(b)
		, a(255)
	{}

	unsigned char r, g, b, a;
};

class Window;
class Widget;

class Application
{
public:
	Application();
	~Application();
	void addWindow(Window *win);
	void mainLoop();

private:
	std::list<Window *> windows;
};

class Texture
{
public:
	Texture()
		: texture(nullptr)
	{}

	Texture(SDL_Texture *texture)
		: texture(texture, SDL_DestroyTexture)
	{}

	operator bool()
	{
		// ternary operator necessary to coerce to bool
		return texture ? true : false;
	}

private:
	friend class Renderer;

	std::shared_ptr<SDL_Texture> texture;
};

/**
 * Everything here is virtual so it can be reimplemented by the user as a subclass,
 * perhaps without even using SDL2.
 */
class Renderer
{
public:
	explicit Renderer(Window *win);
	virtual ~Renderer();
	virtual void rect(Box at, Color color);
	virtual Texture loadImage(const char *file);
	virtual void texture(Texture texture, Box at);
	virtual void clear();
	virtual void present();

private:
	Window *window;
	SDL_Renderer *renderer;
};

class Window
{
	friend class Renderer;

public:
	explicit Window(const char *name);
	~Window();

	template <typename T>
	T &centralWidget()
	{
		if (central)
			return *reinterpret_cast<T *>(central);

		T *widget = new T;
		central = reinterpret_cast<Widget *>(widget);
		return *widget;
	}

	void update();

	Size getSize();

private:
	Renderer *renderer;
	Widget *central = nullptr;
	SDL_Window *window = nullptr;
};

class Widget
{
public:
	virtual ~Widget()
	{}
	
	virtual void render(Box boundingBox, Renderer &renderer);

	template <typename T>
	T &addChild()
	{
		T *child = new T;
		children.push_back(std::shared_ptr(reinterpret_cast<Widget *>(child)));
		return *child;
	}

private:
	std::list<std::shared_ptr<Widget>> children;
};

} // ng::ui
