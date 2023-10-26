import { Injectable } from '@angular/core';
import {ESPLoader, LoaderOptions, Transport} from "esptool-js";
import {NgTerminal} from "ng-terminal";
import {NgxSerial} from "ngx-serial";
import {fromPromise} from "rxjs/internal/observable/innerFrom";
import {EMPTY, empty, Observable, Subject, tap} from "rxjs";

@Injectable({
  providedIn: 'root'
})
export class SerialPortService {

  private device?: any;
  private transport?: Transport;
  private esploader?: ESPLoader;

  private port?: any;
  private readonly serial: NgxSerial;
  private readonly _dataReceived = new Subject<string>();

  constructor() {
    this.serial = new NgxSerial((data: string) => {
      console.log('Received', data);
      this._dataReceived.next(data);
    }, {
      baudRate: 115200
    });
  }

  get dataReceived$(): Observable<string> {
    return this._dataReceived.asObservable();
  }

  connectSerial(): Observable<void> {
    return fromPromise(this.serial.connect((port: any) => {
      this.port = port;
    }));
  }

  sendData(data: string): Observable<void> {
    if (!this.serial) {
      return EMPTY;
    }

    console.log('Send', data);

    return fromPromise(this.serial.sendData(data));
  }

  async connectSerialFlahser(term?: NgTerminal) {
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

  async disconnectSerialFlash(term?: NgTerminal) {
    await this.transport?.disconnect();
    await this.transport?.waitForUnlock(1500);

    this.device = undefined;
    this.transport = undefined;

    term?.underlying?.clear();
  }

  disconnectSerial(): Observable<void>{
    this.port = null;

    if (this.serial) {
      return fromPromise(this.serial.close(() => {
      }));
    }

    return EMPTY;
  }
}
