#!/usr/bin/env python3
"""Run host regression tests; compiles production functions, not copied logic."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]

def function(path, signature):
    text = (root / path).read_text()
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

with tempfile.TemporaryDirectory() as temp:
    executable = str(Path(temp) / 'ble-test')
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-sign-compare',
                    '-Itests/host/stubs', '-Iinclude', 'tests/host/ble_security.cpp',
                    '-o', executable], cwd=root, check=True)
    subprocess.run([executable], check=True)
    code = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
static uint32_t now=20000,displayTime=0;
static bool displayState=false,displayEcoMode=true;
static int menuDisplay=0;
struct {struct {uint32_t timeout=4;} display;} Config;
uint32_t millis(){return now;}
#define SH110X_DISPLAYON 1
#define SH110X_DISPLAYOFF 0
struct {bool on=false; void oled_command(int command){on=command;}} display;
void displayToggle(bool);
'''
    code += function('src/display.cpp', 'void displayWake()') + '\n'
    code += function('src/display.cpp', 'void displayToggle(bool toggle)') + '\n'
    code += function('src/utils.cpp', 'void checkDisplayEcoMode()') + '\n'
    code += r'''
int main(){
 displayWake();assert(display.on && displayState && displayTime==now);
 now+=3999;checkDisplayEcoMode();assert(display.on);
 now++;checkDisplayEcoMode();assert(!display.on && !displayState);
 displayWake();now+=3000;displayWake();now+=3000;
 checkDisplayEcoMode();assert(display.on); // activity renews timeout
 now+=1000;checkDisplayEcoMode();assert(!display.on);
 displayToggle(true);assert(displayState); // button/menu path stays in sync
 now+=4000;checkDisplayEcoMode();assert(!display.on);
 now=UINT32_MAX-1000;displayWake();now=2999;checkDisplayEcoMode();
 // Existing 10-second startup guard defers shutdown across millis wrap.
 now=11000;checkDisplayEcoMode();assert(!display.on);
 puts("PASS: OLED wake, timeout, activity renewal, toggle state, millis wrap");
}
'''
    source = Path(temp) / 'display-test.cpp'
    source.write_text(code)
    executable = str(Path(temp) / 'display-test')
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', str(source), '-o', executable], check=True)
    subprocess.run([executable], check=True)
