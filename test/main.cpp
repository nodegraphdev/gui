#include <ngui.h>
#include <image.h>

using namespace ng::ui;

int main(int argc, char **argv)
{
	Application app;
	Window mainWindow("Node Graph UI test");

	auto &img = mainWindow.centralWidget<Image>();
	img.setImage("/home/ch/Pictures/wallpaper.jpg");

	app.addWindow(&mainWindow);

	app.mainLoop();
}
