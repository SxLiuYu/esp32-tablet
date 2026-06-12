#ifndef AUDIO_H
#define AUDIO_H

void audio_init(void);
void audio_capture_start(void (*cb)(const char *base64, int samples));
void audio_capture_stop(void);
void audio_play_b64_mp3(const char *b64_mp3);

#endif // AUDIO_H