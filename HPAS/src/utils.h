#ifndef UTILS_H_
#define UTILS_H_

extern unsigned int timer_flag;

void set_duration(double duration);
void hpas_sleep(double sleeptime);

long int parse_size(char *input);

#endif

