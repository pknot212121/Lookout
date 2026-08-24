#include "application.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Application* g_app = nullptr;

int main()
{
    g_app = new Application();
    if (!g_app->initialize())
        return 1;
    return 0;
}