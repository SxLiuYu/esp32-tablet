#ifndef WS_CLIENT_H
#define WS_CLIENT_H

void ws_client_init(void);
void ws_client_send_audio_start(void);
void ws_client_send_audio(const char *base64_pcm, int samples);
void ws_client_send_text(const char *text);

#endif // WS_CLIENT_H