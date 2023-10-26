import { Injectable } from '@angular/core';
import {ESPLoader, LoaderOptions, Transport} from "esptool-js";
import {NgTerminal} from "ng-terminal";

@Injectable({
  providedIn: 'root'
})
export class SerialPortService {

  private device?: any;
  private transport?: Transport;
  private esploader?: ESPLoader;

  constructor() { }

  async connectSerial(term?: NgTerminal) {
    if (!this.device) {
      this.device = await (navigator as any).serial.requestPort({});
      if (!this.device) {
        return;
      }

      this.transport = new Transport(this.device);
    }

    const self = this;

    try {
      const flashOptions = {
        transport: this.transport,
        baudrate: 115200,
        terminal: {
          clean() {
            term?.underlying?.clear();
          },
          writeLine(data: any) {
            term?.underlying?.writeln(data);
          },
          write(data: any) {
            term?.write(data);
          },
        },
        debugLogging: true,
      } as LoaderOptions;

      await this.transport?.connect(flashOptions.baudrate);

      while (this.device.readable) {
        const reader = this.device.readable.getReader();
        try {
          while (true) {
            const { value, done } = await reader.read();
            if (done) {
              // |reader| has been canceled.
              break;
            }
            // Do something with |value|...
          }
        } catch (error) {
          // Handle |error|...
        } finally {
          reader.releaseLock();
        }
      }

      // this.esploader = new ESPLoader(flashOptions);

      // this.chip = await this.esploader.main_fn();

      // Temporarily broken
      // await esploader.flash_id();
      term?.underlying?.writeln(`Connected !`);

      // await this.transport?.write(new TextEncoder().encode('r'));
    } catch (e: Error | any) {
      console.error(e);
      term?.underlying?.writeln(`Error: ${e.message}`);
    }
  }

  async disconnectSerial(term?: NgTerminal) {
    await this.transport?.disconnect();
    await this.transport?.waitForUnlock(1500);

    this.device = undefined;
    this.transport = undefined;

    term?.underlying?.clear();
  }
}
