#ifndef DISPLAY_H_
#define DISPLAY_H_

void setup_display();
void display_toggle(bool toggle);
void cleanTFT();

void show_display(const String& header, const String& line1, const String& line2, int wait = 0);
void show_display(const String& header, const String& line1, const String& line2, const String& line3, const String& line4, const String& line5, int wait = 0);

void startupScreen(uint8_t index, const String& version);

#endif