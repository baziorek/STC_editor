/** @file
 * Leaks reported by LeakSanitizer which are not leaks of STC_editor.
 *
 * With AddressSanitizer the process is checked at exit as a whole, not only our own code. Run from a Wayland or X11
 * session the application reports hundreds of kilobytes "leaked" in libraries it only uses, which makes the report
 * useless (and the exit code non-zero), so a real leak of our own would drown in them. The leaks come from:
 *  - Qt WebEngine (Chromium): objects of its thread pool, mojo, ANGLE (EGL), the certificate store, which are still
 *    queued for deletion when the process ends (the stacks start in QtWebEngineCore::WebEngineContext::destroy()),
 *  - fontconfig / expat: the font configuration read by the first font lookup,
 *  - xkbcommon, Qt Wayland client: the keyboard map and compose table of the compositor,
 *  - libudev, libdbus: the device and session bus connection of Qt.
 *
 * A leak is suppressed when any frame of its stack is in one of these libraries, so a leak of ours allocated by
 * these libraries on our behalf is hidden too. What was hidden is listed with LSAN_OPTIONS=print_suppressions=1;
 * when leaked WebEngine objects of ours are suspected, remove the "libQt6WebEngineCore.so" line for a moment.
 *
 * Qt itself (libQt6Core, libQt6Widgets, ...) is deliberately NOT here: leaks of QObjects created by us show up there. */

#if defined(__SANITIZE_ADDRESS__)
#define STC_ADDRESS_SANITIZER_ENABLED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define STC_ADDRESS_SANITIZER_ENABLED 1
#endif
#endif

#ifdef STC_ADDRESS_SANITIZER_ENABLED

// Called by the sanitizer runtime before the first report; added to the suppressions given in LSAN_OPTIONS.
extern "C" const char* __lsan_default_suppressions()
{
    return "leak:libQt6WebEngineCore.so\n"
           "leak:libfontconfig.so\n"
           "leak:libexpat.so\n"
           "leak:libxkbcommon.so\n"
           "leak:libQt6WaylandClient.so\n"
           "leak:libudev.so\n"
           "leak:libdbus-1.so\n";
}

#endif // STC_ADDRESS_SANITIZER_ENABLED
