#pragma once

#include <cstdint>
#include <functional>

// "Online Lookup: Keep Wi-Fi On" (Settings > Reader, experimental).
//
// Every other network feature of this firmware restarts into a network-only
// boot and back. With this setting on, Wi-Fi is joined in the background while
// a book is open instead, so the online dictionary can fetch a word in place.
// The price is battery: the radio stays associated for as long as the book is
// open. The dictionary still falls back to the restart route whenever Wi-Fi is
// not there (no saved network, out of range, too little memory).
namespace OnlineWifi {

// The setting is on, on a board that has the memory for Wi-Fi next to a book.
bool enabled();
// A book was opened: tick() joins the last network once its first page is up.
void readerOpened();
// The book was closed: Wi-Fi off again, if this module switched it on.
void readerClosed();
// From the reader's loop: joins in the background, retries a lost connection
// now and then, and lets go of Wi-Fi when the setting has been switched off.
void tick();
// Control Center Wi-Fi button: joins the last network now and keeps Wi-Fi on,
// book or not, until switched off here or the device sleeps.
void setManual(bool on);
bool manualOn();
// The Wi-Fi list connected by hand: keep that connection as the manual one.
void adoptConnection();
// For a lookup: waits until Wi-Fi is connected, joining first if need be.
// givenUp is polled while waiting; true ends the wait at once (as not connected).
bool waitConnected(uint32_t timeoutMs, const std::function<bool()>& givenUp = nullptr);

}  // namespace OnlineWifi
