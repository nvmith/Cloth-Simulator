#include "App.h"

int main()
{
    App app(1280, 720);
    if (!app.init()) return -1;
    app.run();
    return 0;
}
