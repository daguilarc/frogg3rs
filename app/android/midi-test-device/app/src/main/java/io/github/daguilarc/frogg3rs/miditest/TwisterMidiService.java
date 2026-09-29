package io.github.daguilarc.frogg3rs.miditest;

import android.media.midi.MidiDeviceService;
import android.media.midi.MidiReceiver;
import android.util.Log;

import java.io.IOException;

// Test-only (task 4.8): the system starts and binds this service once a
// client (Frogg3rs, opening the Twister preset's input/output) opens the
// device device_info.xml declares. It carries no real instrument -- it only
// logs whatever arrives on its one input port (what Frogg3rs sends out,
// "DJ TechTools Midi Fighter Twister Input Port 1") and, on request
// (SendTurnReceiver), writes a relative encoder-0 turn out its one output
// port ("...Output Port 1", what Frogg3rs reads as its input).
public class TwisterMidiService extends MidiDeviceService {
    static final String TAG = "TwisterMidiTest";

    // MidiDeviceService instances are created and destroyed by the system as
    // clients open and close the device; this test-only static handle is how
    // SendTurnReceiver (a separate manifest component with no other channel
    // to the running instance) reaches whichever one is currently bound, if
    // any. A stale reference matters only if delivery of the "device closed"
    // callback and the receiver's read of this field could interleave, which
    // is checked by nulling it in onDestroy() before that method returns and
    // reading it only under this class's own lock.
    private static final Object INSTANCE_LOCK = new Object();
    private static TwisterMidiService activeInstance;

    // MidiDeviceService.onCreate() (android-37.0 sources, AOSP) calls
    // onGetInputPortReceivers() itself,
    // synchronously, as part of super.onCreate() -- to build the
    // MidiDeviceServer it registers with MidiService, once, for the life of
    // this service instance. Assigning inputReceivers AFTER calling
    // super.onCreate() (as this class used to) meant onGetInputPortReceivers()
    // ran while the field was still null, so MidiService registered this
    // device with no working input port receiver at all -- every
    // openInputPort(0) from any client, in any process, failed silently and
    // permanently for that instance, with no exception anywhere. A field
    // initializer runs during construction, strictly before the system ever
    // calls onCreate(), so onGetInputPortReceivers() always sees a real array.
    private MidiReceiver[] inputReceivers = new MidiReceiver[] { new LoggingReceiver() };

    @Override
    public void onCreate() {
        super.onCreate();
        synchronized (INSTANCE_LOCK) {
            activeInstance = this;
        }
        Log.i(TAG, "device opened");
    }

    @Override
    public void onDestroy() {
        synchronized (INSTANCE_LOCK) {
            if (activeInstance == this) {
                activeInstance = null;
            }
        }
        Log.i(TAG, "device closed");
        super.onDestroy();
    }

    @Override
    public MidiReceiver[] onGetInputPortReceivers() {
        return inputReceivers;
    }

    // Sends one relative encoder-0 turn: Control Change, channel 0 (status
    // 0xB0), controller 0 (EncoderPositionToCC(0) in MidiController.cpp),
    // value 64 + delta (EncoderMidiInProcessor::DecodeTicks's Signed7Bit
    // decode: 65 is +1 tick, 63 is -1). Returns false (and logs why) when no
    // client currently has the device open, so a caller can tell "not sent"
    // from "sent, no answer yet".
    boolean sendTurn(int delta) {
        MidiReceiver[] outputReceivers = getOutputPortReceivers();
        if (outputReceivers == null || outputReceivers.length == 0 || outputReceivers[0] == null) {
            Log.w(TAG, "sendTurn: no output port receiver (device not open by any client)");
            return false;
        }
        byte value = (byte) (64 + delta);
        byte[] message = { (byte) 0xB0, 0x00, value };
        long timestamp = System.nanoTime();
        try {
            outputReceivers[0].send(message, 0, message.length, timestamp);
            Log.i(TAG, String.format("output tx ts=%d delta=%d value=%d bytes=%s",
                    timestamp, delta, value & 0xFF, toHex(message)));
            return true;
        } catch (IOException e) {
            Log.w(TAG, "sendTurn: send failed", e);
            return false;
        }
    }

    static boolean requestTurn(int delta) {
        TwisterMidiService instance;
        synchronized (INSTANCE_LOCK) {
            instance = activeInstance;
        }
        if (instance == null) {
            Log.w(TAG, "requestTurn: no active device instance (nothing has opened it yet)");
            return false;
        }
        return instance.sendTurn(delta);
    }

    static String toHex(byte[] bytes) {
        StringBuilder builder = new StringBuilder();
        for (byte b : bytes) {
            if (builder.length() > 0) {
                builder.append(' ');
            }
            builder.append(String.format("%02X", b));
        }
        return builder.toString();
    }

    // What Frogg3rs sends back on its output (received here on our one input
    // port): the ring/position feedback TwisterMidiOutProcessor::Process
    // writes is also channel 0 (kPrimaryPositionChannel), controller 0
    // (mapping.cc for encoder 0) -- same status byte 0xB0 0x00, a different
    // value each time the encoder's displayed position changes. Logging
    // every byte, not just that address, is what lets 4.8's report show the
    // names JUCE actually used if the round trip fails.
    private static final class LoggingReceiver extends MidiReceiver {
        @Override
        public void onSend(byte[] msg, int offset, int count, long timestamp) throws IOException {
            byte[] copy = new byte[count];
            System.arraycopy(msg, offset, copy, 0, count);
            Log.i(TAG, String.format("input rx ts=%d recvNanos=%d bytes=%s",
                    timestamp, System.nanoTime(), toHex(copy)));
        }
    }
}
