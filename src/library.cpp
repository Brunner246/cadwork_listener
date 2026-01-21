#include <cwapi3d/CwAPI3D.h>
#include "ServerHandler.h"

CWAPI3D_PLUGIN bool plugin_x64_init(CwAPI3D::ControllerFactory *aFactory)
{
    const auto serverHandler = ServerHandler(aFactory->getUtilityController());
    serverHandler.runEventLoop();

    return false;
}

