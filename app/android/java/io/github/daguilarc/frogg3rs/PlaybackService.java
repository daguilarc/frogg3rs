package io.github.daguilarc.frogg3rs;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;

// design.md, "Lifecycle": a foreground service is what keeps JUCE's audio
// device open, MIDI callbacks and ring-output timer running with the screen
// off, by keeping the process out of the cached/frozen state and its audio
// unsilenced. It carries no audio code of its own -- Frogg3rsActivity starts
// and stops it; JUCE's own Oboe/AAudio device (opened from C++, unrelated to
// this class) is what actually plays and records.
public class PlaybackService extends Service {
    private static final String CHANNEL_ID = "frogg3rs_playback";
    private static final int NOTIFICATION_ID = 1;

    // The audio system's own wake lock is documented for playing audio but
    // not verified here for an exclusive stream that keeps writing silence
    // between notes (design.md), so this service holds its own partial wake
    // lock for its whole lifetime instead of relying on that.
    private PowerManager.WakeLock wakeLock;

    @Override
    public void onCreate() {
        super.onCreate();
        PowerManager powerManager = (PowerManager) getSystemService(POWER_SERVICE);
        wakeLock = powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "frogg3rs:playback");
        wakeLock.acquire();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        startForeground(NOTIFICATION_ID, buildNotification(), foregroundServiceType());
        return START_STICKY;
    }

    // mediaPlayback always; microphone only once RECORD_AUDIO is granted --
    // starting a microphone-typed foreground service without that permission
    // throws (design.md), and Frogg3rsActivity's onResume call into this
    // method again is what lets a permission granted while the app was away
    // take effect without a process restart.
    private int foregroundServiceType() {
        int type = ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK;
        boolean hasRecordAudio = checkSelfPermission(android.Manifest.permission.RECORD_AUDIO)
                == PackageManager.PERMISSION_GRANTED;
        if (hasRecordAudio) {
            type |= ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
        }
        return type;
    }

    private Notification buildNotification() {
        NotificationManager notificationManager = getSystemService(NotificationManager.class);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                    CHANNEL_ID, "Frogg3rs playback", NotificationManager.IMPORTANCE_LOW);
            notificationManager.createNotificationChannel(channel);
        }

        Intent openApp = new Intent(this, Frogg3rsActivity.class);
        openApp.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        PendingIntent contentIntent = PendingIntent.getActivity(
                this, 0, openApp, PendingIntent.FLAG_IMMUTABLE);

        return new Notification.Builder(this, CHANNEL_ID)
                .setContentTitle("Frogg3rs")
                .setContentText("Running")
                .setSmallIcon(R.drawable.icon)
                .setContentIntent(contentIntent)
                .setOngoing(true)
                .build();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onDestroy() {
        if (wakeLock != null && wakeLock.isHeld()) {
            wakeLock.release();
        }
        super.onDestroy();
    }
}
