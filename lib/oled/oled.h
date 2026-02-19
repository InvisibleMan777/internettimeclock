#ifndef OLED_H
#define OLED_H

int initialize_oled(void);
int write_to_oled(const char *line1, const char *line2);

#endif