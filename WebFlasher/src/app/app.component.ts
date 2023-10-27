import {Component, OnDestroy, OnInit, ViewChild} from '@angular/core';
import {NgTerminal} from "ng-terminal";
import {SerialPortService} from "./shared/services/serial-port.service";
import {Subject, Subscription} from "rxjs";
import {NotifierService} from "angular-notifier";
import {Settings} from "./shared/models/settings.interface";

@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.scss']
})
export class AppComponent implements OnInit, OnDestroy {

  @ViewChild('term') term?: NgTerminal;

  newSettingsFormData = new Subject<Settings>();

  private readonly subscriptions = new Subscription();

  constructor(public readonly serialPortService: SerialPortService,
              private readonly toastService: NotifierService) {
  }

  ngOnInit(): void {
    this.subscriptions.add(this.serialPortService.dataReceived$.subscribe({
      next: data => {
        this.term?.write(data);
        this.term?.write("\n");

        if (data[0] === 'g') {
          const jsonData = JSON.parse(data.substring(1));
          this.newSettingsFormData.next(jsonData);
          this.toastService.show({
            type: 'info',
            message: 'Configuration loaded from ESP',
          });
        } else if (data[0] === 's') {
          if (data[1] === '0') {
            this.toastService.show({
              type: 'error',
              message: 'Error during transfert to ESP',
            });
          } else {
            this.toastService.show({
              type: 'success',
              message: 'Transfer successful to ESP',
            });
          }
        }
      }
    }));
  }

  ngOnDestroy() {
    this.subscriptions.unsubscribe();
  }

  connectSerial() {
    this.subscriptions.add(this.serialPortService.connectSerial().subscribe({
      next: () => {
        this.serialPortService.sendData("g\n");
        this.toastService.show({
          type: 'success',
          message: 'Connected to ESP',
        });
      },
      error: err => {
        this.toastService.show({
          type: 'error',
          message: 'Error during Serial connection',
        });
      }
    }));
  }

  disconnectSerial() {
    this.subscriptions.add(this.serialPortService.disconnectSerial().subscribe({
      next: () => {
        this.toastService.show({
          type: 'success',
          message: 'Disconnected from ESP',
        });
      },
      error: err => {
        this.toastService.show({
          type: 'error',
          message: 'Error during Serial disconnection',
        });
      }
    }));
  }

  saveSettings(value: Settings) {
    this.subscriptions.add(
      this.serialPortService.sendData(`s${JSON.stringify(value)}`).subscribe({
        error: err => {
          this.toastService.show({
            type: 'error',
            message: 'Error during Serial transfert to ESP',
          });
        }
      })
    );
  }
}
