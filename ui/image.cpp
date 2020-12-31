#include <image.h>
#include <iostream>

namespace ng::ui
{

void Image::render(Box boundingBox, Renderer &renderer)
{
	std::cerr << boundingBox.w << " " << boundingBox.h << std::endl;

	if (!textureLoaded)
	{
		auto img = get<std::string>("src");
		texture = renderer.loadImage(img.c_str());
		textureLoaded = true;
	}

	renderer.texture(texture, boundingBox);
}

}
