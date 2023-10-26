import {NgModule} from '@angular/core';
import {BrowserModule} from '@angular/platform-browser';

import {AppComponent} from './app.component';
import {ReactiveFormsModule} from "@angular/forms";
import {NgTerminalModule} from "ng-terminal";
import {SerialPortService} from "./serial-port.service";

@NgModule({
  declarations: [
    AppComponent
  ],
  imports: [
    BrowserModule,
    ReactiveFormsModule,
    NgTerminalModule
  ],
  providers: [
    SerialPortService
  ],
  bootstrap: [AppComponent]
})
export class AppModule { }
