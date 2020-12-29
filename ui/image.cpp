#include <image.h>

namespace ng::ui
{

void Image::setImage(const char *path)
{
	imagePath = path;
	textureLoaded = false;
}

void Image::render(Box boundingBox, Renderer &renderer)
{
	if (!textureLoaded)
	{
		texture = renderer.loadImage(imagePath.c_str());
		textureLoaded = true;
	}

	renderer.texture(texture, boundingBox);
}

}