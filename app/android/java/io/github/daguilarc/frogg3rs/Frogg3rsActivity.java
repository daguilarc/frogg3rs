package io.github.daguilarc.frogg3rs;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

// design.md, "Lifecycle": what keeps JUCE's audio device open, MIDI callbacks
// and ring-output timer alive with the screen off is the process staying out
// of the cached/frozen state background apps eventually reach, or its
// background audio being silenced -- a foreground service (PlaybackService)
// prevents both. This activity's only job is to keep that service's
// lifecycle attached to the activity's: started in onCreate AND again in
// onResume (a permission granted while the app was away, e.g. microphone,
// only takes effect on the service's NEXT startForeground call -- onResume
// is what lets the service's foreground type pick that up without waiting
// for the process to restart), stopped in onDestroy before this activity's
// own teardown exits the process, so the service never outlives the
// activity that owns it. Extends android.app.Activity directly, not JUCE's
// own com.rmsl.juce.JuceActivity: that class's onResume calls the native
// appOnResume, which JUCE compiles only when JUCE_PUSH_NOTIFICATIONS_ACTIVITY
// is defined (juce_Windowing_android.cpp), and Projucer defines that only
// with push notifications enabled (jucer_ProjectExport_Android.h
// getActivityClassString, which returns plain android.app.Activity itself
// for an app without them and no custom class) -- with push notifications
// off, calling through JuceActivity crashed this app on its first resume
// with an UnsatisfiedLinkError. JUCE's own window and native init do not
// depend on the activity subclass.
public class Frogg3rsActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        startService(new Intent(this, PlaybackService.class));
    }

    @Override
    protected void onResume() {
        super.onResume();
        startService(new Intent(this, PlaybackService.class));
    }

    @Override
    protected void onDestroy() {
        stopService(new Intent(this, PlaybackService.class));
        super.onDestroy();
    }
}
