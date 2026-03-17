#include "app/application.h"
#include "app/main_window.h"

int main(int argc, char* argv[])
{
    microbotica::app::Application app(argc, argv);

    microbotica::app::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
