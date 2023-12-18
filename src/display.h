#ifndef DISPLAY_H_
#define DISPLAY_H_

void setup_display();
void display_toggle(bool toggle);

void show_display(String header, int wait = 0);
void show_display(String header, String line1, int wait = 0);
void show_display(String header, String line1, String line2, int wait = 0);
void show_display(String header, String line1, String line2, String line3, int wait = 0);
void show_display(String header, String line1, String line2, String line3, String line4, int wait = 0);
void show_display(String header, String line1, String line2, String line3, String line4, String line5, int wait = 0);

#endif