#pragma once

namespace ng::ui
{

struct Point
{
	int x, y;
};

struct Box
{
	int x, y, w, h;
};

struct Color
{
	unsigned char r, g, b, a;
};

class Application
{
public:
	Application();
	void mainLoop();
};

class Widget;

class Window
{
	friend class Renderer;

public:
	Window();
	~Window();

	template <typename T>
	T &centralWidget()
	{
		if (central)
			return *reinterpret_cast<T *>(central);

		T *widget = new T;
		central = reinterpret_cast<Widget *>(widget);
	}

	void update();

private:
	Widget *central;
};

class Renderer
{
public:
	Renderer();
	virtual void rect(Box at, Color color);
};

class Widget
{
public:
	virtual void render(Box &boundingBox, Renderer &renderer);
};

}