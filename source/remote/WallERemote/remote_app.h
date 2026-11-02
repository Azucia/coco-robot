#pragma once
void remoteSetup();
void remoteLoop();

// Call from the existing UI task: no button placement is imposed yet.
// true means queued locally, not a playback acknowledgement.
bool remotePlayDialogue(unsigned slot);  // 1..16; currently only slot 1 has audio
bool remotePlayNextDialogue();
bool remoteStopDialogue();
