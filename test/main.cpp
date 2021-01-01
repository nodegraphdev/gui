#include <ngui.h>
#include <image.h>

using namespace ng::ui;

int main(int argc, char **argv)
{
	Application app;
	// TODO: do this automatically
	app.registerWidget<Widget>("Widget");
	app.registerWidget<Image>("Image");

	Window mainWindow("Node Graph UI test");

	mainWindow.setCentralWidget(app.fromFile("../test/MainWindow.xml"));

	app.addWindow(&mainWindow);
	app.mainLoop();
}
