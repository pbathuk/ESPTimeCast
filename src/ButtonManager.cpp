#include "ButtonManager.h"

void handleLongClick(Button2& b) {
    // Can use b to map the buttons, since I know the order they begin it will be :
    // 0: left, 1: middle, 2: right
    printf("Button Long Clicked down!");
    printf("Button ID: %d\n", b.getID());
    if (!(dimActive && dimmingClicks)) {
        if (strlen(longClickSound) > 0) {// Anti-Pop Fix: Mute before initialization
            audio.setVolume(0);
            audio.connecttoFS(LittleFS, longClickSound);
            // Restore Volume immediately (The library applies the mute during the init phase)
            audio.setVolume(audioVolume);
        }
    }
}

void handleShortClick(Button2& b) {
    printf("Button Short Clicked down! ID: %d\n", b.getID());

    if (!(dimActive && dimmingClicks)) {
        if (strlen(shortClickSound) > 0) {
            
            // 1. Force Stop & Clean Cleanup
            if (audio.isRunning()) {
                audio.stopSong(); 
                delay(10); // Crucial: Give LittleFS 10ms to close the file handle
            }

            // 2. Anti-Pop Mute
            audio.setVolume(0);

            // 3. Play
            audio.connecttoFS(LittleFS, shortClickSound);

            // 4. Restore Volume
            audio.setVolume(audioVolume);
        }
    }
}
