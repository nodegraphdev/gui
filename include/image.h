#pragma once

#include <ngui.h>

namespace ng::ui
{

class Image : public Widget
{
public:
	Image() : Widget()
	{}

	virtual void render(Box boundingBox, Renderer &renderer) override;

private:
	bool textureLoaded = false;
	Texture texture;
};

}
