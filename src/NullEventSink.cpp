#include "NullEventSink.h"

void NullEventSink::emitEvent(const RunEvent & /*event*/)
{
}

void NullEventSink::close()
{
}

bool NullEventSink::isOpen() const
{
    return false;
}
