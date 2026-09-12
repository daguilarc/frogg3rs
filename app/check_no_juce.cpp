// check_no_juce.cpp -- mechanically enforces "no JUCE in the
// app core."
//
// This TU includes only Froggers.hpp (the app core) and is compiled with
// JUCE's module directory ADDED to the include path (see the
// check-no-juce Makefile target) -- deliberately, so that "we simply never
// pointed the compiler at JUCE" cannot masquerade as passing this check.
// Absence of a link dependency is not sufficient;
// this proves no header Froggers.hpp
// transitively includes resolves into JUCE, by asserting the sentinel macro
// every JUCE header sets (JUCE_MAJOR_VERSION, set by
// juce_core/system/juce_TargetPlatform.h) is still undefined afterwards.
// Modelled on the same pattern Sheaf's own
// External/Sheaf/projects/synth/tests/miniapp_system_tests.cpp:15-17 uses to guard MiniAppCore.

#include "Froggers.hpp"

#ifdef JUCE_MAJOR_VERSION
#error "Froggers.hpp (the app core) must not resolve any JUCE header"
#endif

int main() { return 0; }
