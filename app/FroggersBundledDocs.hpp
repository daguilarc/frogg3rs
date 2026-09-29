#pragma once

// Opens one of the app's bundled operator documents (MANUAL.md or
// QUICK_DICT.md) with the operating system's default handler for that file
// type -- no in-app text viewer, no network.
//
// On macOS, works unmodified from both the standalone launcher and the
// plugin editor: juce::File::currentApplicationFile resolves to the CALLING
// module's own bundle root via dladdr (JUCE's juce_Files_mac.mm, the
// currentApplicationFile case: it walks up from the executable file to the
// enclosing bundle root whenever the executable sits under a
// "Contents/MacOS" directory, which is true of an .app, a .vst3, and a
// .component alike) -- the standalone app bundle for the launcher, the
// .vst3 or .component bundle for the plugin. Both hosts therefore read the
// SAME two files, copied into each bundle's Contents/Resources at build
// time from the repository's single copy (see app/build-launcher.sh and
// app/vst/CMakeLists.txt).
//
// Elsewhere (the standalone launcher only -- the plugin editors ship
// macOS-only), juce::File::currentApplicationFile IS the executable file
// itself rather than a bundle root, so the document is resolved beside it
// instead, in its parent directory, where app/standalone/CMakeLists.txt
// copies the same two files from the repository's single copy.
//
// Either way, there is no second checked-in copy of either document
// anywhere for the bundled ones to drift from.

#include <juce_core/juce_core.h>

namespace frogg3rs_docs {

inline void OpenBundledDoc(const char* filename) {
#if JUCE_MAC
    const juce::File bundleRoot = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    const juce::File doc = bundleRoot.getChildFile("Contents/Resources").getChildFile(filename);
#else
    const juce::File exeDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();
    const juce::File doc = exeDir.getChildFile(filename);
#endif
    doc.startAsProcess();
}

}  // namespace frogg3rs_docs
