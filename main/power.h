#ifndef POWER_H
#define POWER_H

void power_init(void);
int  power_get_battery_pct(void);
void power_check_sleep(void);

#endif // POWER_H