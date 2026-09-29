package io.github.daguilarc.frogg3rs;

import android.content.Intent;
import android.os.Bundle;

import com.rmsl.juce.JuceActivity;

// design.md, "Lifecycle": what keeps JUCE's audio device open, MIDI callbacks
// and ring-output timer alive with the screen off is the process staying out
// of the cached/frozen state background apps eventually reach, or its
// background audio being silenced -- a foreground service (PlaybackService)
// prevents both. This activity's only job is to keep that service's
// lifecycle attached to the activity's: started in onCreate AND again in
// onResume (a permission granted while the app was away, e.g. microphone,
// only takes effect on the service's NEXT startForeground call -- onResume
// is what lets the service's foreground type pick that up without waiting
// for the process to restart), stopped in onDestroy before JUCE's own
// teardown (the JuceActivity superclass) runs and exits the process, so the
// service never outlives the activity that owns it.
public class Frogg3rsActivity extends JuceActivity {
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
