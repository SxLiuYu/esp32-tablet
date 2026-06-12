#ifndef WIFI_H
#define WIFI_H

#include <stdint.h>

void wifi_init_sta(void);
void wifi_init_softap(void);
int  wifi_is_connected(void);

#endif // WIFI_H