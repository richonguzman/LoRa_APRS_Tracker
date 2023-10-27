import {NgModule} from '@angular/core';
import {BrowserModule} from '@angular/platform-browser';

import {AppComponent} from './app.component';
import {ReactiveFormsModule} from "@angular/forms";
import {NgTerminalModule} from "ng-terminal";
import {SerialPortService} from "./shared/services/serial-port.service";
import {NotifierModule} from "angular-notifier";
import { SettingsFormComponent } from './settings-form/settings-form.component';

@NgModule({
  declarations: [
    AppComponent,
    SettingsFormComponent
  ],
  imports: [
    BrowserModule,
    ReactiveFormsModule,
    NgTerminalModule,
    NotifierModule.withConfig({
      behaviour: {
        autoHide: 2500
      },
      position: {
        horizontal: {
          position: 'middle'
        }
      }
    }),
  ],
  providers: [
    SerialPortService
  ],
  bootstrap: [AppComponent]
})
export class AppModule { }
