package io.github.daguilarc.frogg3rs.miditest;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiInputPort;
import android.media.midi.MidiManager;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;

import java.io.IOException;

// A positive control: proves this virtual device's own input port -- and
// TwisterMidiService's LoggingReceiver, which logs every byte under tag
// TwisterMidiTest -- can receive a message at all, sent by a client that is
// not Frogg3rs and does not go through Sheaf or JUCE. This process
// (io.github.daguilarc.frogg3rs.miditest) opens the device through the
// ordinary android.media.midi.MidiManager client API, exactly as any other
// app (including Frogg3rs) would, and writes one message to its input port:
//   adb shell am broadcast --receiver-include-background \
//       --include-stopped-packages -a \
//       io.github.daguilarc.frogg3rs.miditest.POKE_INPUT
// (both flags matter: the first bypasses the background-execution limit on
// manifest-declared receivers, the second bypasses the separate "stopped"
// state a freshly installed, never-launched package starts in, which blocks
// every broadcast to it regardless of the first flag.) Logs "poke: opened",
// "poke: wrote N bytes", or "poke: <failure>" under tag TwisterMidiTest. A
// successful "input rx" line from LoggingReceiver following "poke: wrote"
// isolates a MIDI-output bug to Frogg3rs/Sheaf/JUCE's own output path, not
// to this test harness or to Android's MIDI routing.
// MidiManager.openDevice()'s callback runs on the Handler passed in (or, if
// null, on the calling thread's own Looper) -- a BroadcastReceiver's
// onReceive() runs on the main thread, so blocking there while waiting for a
// main-thread-posted callback would deadlock. goAsync() plus a dedicated
// HandlerThread keeps every step off the main thread and lets onReceive()
// return immediately while the open/write/close chain completes later.
public class PokeInputPortReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context contextIn, Intent intentUnused) {
        final Context context = contextIn.getApplicationContext();
        final PendingResult pendingResult = goAsync();
        final HandlerThread thread = new HandlerThread("PokeInputPort");
        thread.start();
        final Handler handler = new Handler(thread.getLooper());
        // finishAndQuit is passed all the way down the async chain and is
        // called exactly once, only once the chain has truly finished (open
        // failed, port failed, send completed or failed) -- never right
        // after posting the initial openDevice() call, which is itself
        // asynchronous and would otherwise leave nothing alive to run its
        // callback on.
        final Runnable finishAndQuit = () -> {
            pendingResult.finish();
            thread.quitSafely();
        };

        handler.post(() -> poke(context, handler, finishAndQuit));
    }

    private static void poke(Context context, Handler handler, Runnable finishAndQuit) {
        MidiManager manager = (MidiManager) context.getSystemService(Context.MIDI_SERVICE);
        if (manager == null) {
            Log.w(TwisterMidiService.TAG, "poke: no MidiManager");
            finishAndQuit.run();
            return;
        }

        MidiDeviceInfo target = null;
        for (MidiDeviceInfo info : manager.getDevices()) {
            String product = info.getProperties().getString(MidiDeviceInfo.PROPERTY_PRODUCT);
            if ("Midi Fighter Twister".equals(product)) {
                target = info;
                break;
            }
        }
        if (target == null) {
            Log.w(TwisterMidiService.TAG, "poke: device not found in MidiManager.getDevices()");
            finishAndQuit.run();
            return;
        }

        manager.openDevice(target, device -> {
            if (device == null) {
                Log.w(TwisterMidiService.TAG, "poke: openDevice returned null (device busy or gone)");
                finishAndQuit.run();
                return;
            }

            Log.i(TwisterMidiService.TAG, "poke: opened");
            MidiInputPort inputPort = device.openInputPort(0);
            if (inputPort == null) {
                Log.w(TwisterMidiService.TAG, "poke: openInputPort(0) returned null");
                closeQuietly(device);
                finishAndQuit.run();
                return;
            }

            Log.i(TwisterMidiService.TAG, "poke: openInputPort(0) succeeded");
            byte[] message = {(byte) 0xB0, 0x00, 0x41};
            try {
                inputPort.send(message, 0, message.length);
                Log.i(TwisterMidiService.TAG, "poke: wrote " + message.length + " bytes");
            } catch (IOException e) {
                Log.w(TwisterMidiService.TAG, "poke: send failed", e);
            } finally {
                closeQuietly(inputPort);
                closeQuietly(device);
                finishAndQuit.run();
            }
        }, handler);
    }

    private static void closeQuietly(MidiInputPort port) {
        try {
            port.close();
        } catch (IOException ignored) {
        }
    }

    private static void closeQuietly(MidiDevice device) {
        try {
            device.close();
        } catch (IOException ignored) {
        }
    }
}
