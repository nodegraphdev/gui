#include <ngui.h>

using namespace ng::ui;

int main(int argc, char **argv)
{
	Application app;
	Window mainWindow("Node Graph UI test");

	auto &central = mainWindow.centralWidget<Widget>();

	app.addWindow(&mainWindow);

	app.mainLoop();
}
