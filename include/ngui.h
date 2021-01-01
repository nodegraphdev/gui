#pragma once

#include <list>
#include <unordered_map>
#include <memory>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <functional>
#include <utility>
#include <pugixml.hpp>

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

	[[noreturn]]
	void mainLoop();

	template <typename T>
	void registerWidget(std::string name)
	{
		constructors[std::move(name)] = []() -> Widget *
		{
			return dynamic_cast<Widget *>(new T);
		};
	}

	template <typename T>
	T *widgetByName(const std::string &name)
	{
		if (constructors.find(name) == constructors.end())
		{
			throw std::invalid_argument("Cannot find widget with name '" + name + "'");
		}
		return dynamic_cast<T *>(constructors[name]());
	}

	Widget *fromFile(const char *path);
	Widget *widgetFromMarkup(pugi::xml_node doc);
	void populateFromMarkup(Widget *widget, pugi::xml_node doc);

private:
	std::list<Window *> windows;
	std::unordered_map<std::string, std::function<Widget *(void)>> constructors;
};

class Texture
{
public:
	Texture()
		: texture(nullptr)
	{}

	explicit Texture(SDL_Texture *texture)
		: texture(texture, SDL_DestroyTexture)
	{}

	operator bool()
	{
		// ternary operator necessary to coerce to bool
		return static_cast<bool>(texture);
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
	virtual void texture(const Texture &texture, Box at);
	virtual void clear();
	virtual void present();

private:
	friend class Window;

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
			return *dynamic_cast<T *>(central);

		T *widget = new T;
		central = dynamic_cast<Widget *>(widget);
		return *widget;
	}

	void setCentralWidget(Widget *widget)
	{
		central = widget;
	}

	void update();

	Size getSize();

private:
	Renderer *renderer;
	Widget *central = nullptr;
	SDL_Window *window = nullptr;
};

class Property
{
public:
	Property() = default;

	explicit Property(std::string value)
		: value(std::move(value))
	{}

	Property &operator=(std::string other)
	{
		value = std::move(other);
		changed();
		return *this;
	}

	Property &operator=(int other)
	{
		value = std::to_string(other);
		changed();
		return *this;
	}

	Property &operator=(bool other)
	{
		value = std::to_string(other);
		changed();
		return *this;
	}

	explicit operator bool()
	{
		return value.length() > 0 && value != "false";
	}

	explicit operator int()
	{
		return std::stoi(value);
	}

	explicit operator std::string()
	{
		return value;
	}

	void onChange(std::function<void (std::string)> listener)
	{
		listeners.push_back(std::move(listener));
	}

private:
	std::string value;
	std::list<std::function<void (std::string)>> listeners;

	void changed()
	{
		for (const auto &f : listeners)
		{
			f(value);
		}
	}
};

class Widget
{
public:
	virtual ~Widget() = default;
	
	virtual void render(Box boundingBox, Renderer &renderer);

	template <typename T>
	T &newChild()
	{
		T *child = new T;
		children.push_back(std::shared_ptr<Widget>(dynamic_cast<Widget *>(child)));
		return *child;
	}

	template <typename T>
	void addChild(T *child)
	{
		children.push_back(std::shared_ptr<Widget>(dynamic_cast<Widget *>(child)));
	}

	void set(const std::string &key, std::string val)
	{
		properties[key] = std::move(val);
	}

	void onChange(const std::string &key, std::function<void (std::string)> f)
	{
		properties[key].onChange(std::move(f));
	}

private:
	std::list<std::shared_ptr<Widget>> children;
	std::unordered_map<std::string, Property> properties;

protected:
	template <typename T>
	T get(const std::string &key)
	{
		return static_cast<T>(properties[key]);
	}
};

} // ng::ui
