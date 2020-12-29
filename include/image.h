#pragma once

#include <ngui.h>

namespace ng::ui
{

class Image : Widget
{
public:
	Image() : Widget()
	{}

	void setImage(const char *path);

	virtual void render(Box boundingBox, Renderer &renderer) override;

private:
	std::string imagePath;
	bool textureLoaded = false;
	Texture texture;
};

}
