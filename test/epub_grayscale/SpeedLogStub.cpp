#include <SpeedLog.h>

// The speed log appends to the SD card on device; these tests only need the symbol.
void SpeedLog::record(SpeedLog::Event, uint32_t, int32_t, int32_t) {}
