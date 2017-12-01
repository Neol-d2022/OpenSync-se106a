#include <stdio.h>

#include "config.h"
#include "configurer_test.h"
#include "filetree_test.h"
#include "mm_test.h"
#include "strings_test.h"
#include "xsocket.h"

static int _selfTest(void)
{
    if (mm_test())
        return 1;
    if (strings_test())
        return 1;
    if (filetree_test())
        return 1;
    if (configurer_test())
        return 1;
    if (socketLibInit())
        return 1;
    return 0;
}

static void _clearUp(void)
{
    socketLibDeInit();
}

int main(void)
{
    printf("Hello, I am %s. I am currently under construction.\n", PACKAGE_STRING);
    if (_selfTest())
    {
        printf("Self test failed. Exiting now.\n");
        return 1;
    }
    _clearUp();
    return 0;
}
