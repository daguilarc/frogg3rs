package io.github.daguilarc.frogg3rs.miditest;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

// Test-only control surface for task 4.8's emulator checks:
//   adb shell am broadcast -a io.github.daguilarc.frogg3rs.miditest.SEND_TURN --ei delta 1
// sends one relative encoder-0 turn (delta +1 or -1; defaults to +1) out the
// virtual device's output port. This is the only way anything outside the
// process can trigger a send -- the device itself has no UI.
public class SendTurnReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        int delta = intent.getIntExtra("delta", 1);
        boolean sent = TwisterMidiService.requestTurn(delta);
        Log.i(TwisterMidiService.TAG, "SendTurnReceiver: delta=" + delta + " sent=" + sent);
    }
}
