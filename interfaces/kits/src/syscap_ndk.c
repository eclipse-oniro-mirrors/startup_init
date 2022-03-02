#include <stdbool.h>
#include "systemcapability.h"
#include "syscap_ndk.h"

bool canIUse(const char *cap)
{
    return HasSystemCapability(cap);
}