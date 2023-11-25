import {Component, EventEmitter, Input, OnDestroy, OnInit, Output} from '@angular/core';
import {FormArray, FormControl, FormGroup, Validators} from "@angular/forms";
import {debounceTime, filter, Subject, Subscription} from "rxjs";
import {CookieService} from "ngx-cookie-service";
import {NotifierService} from "angular-notifier";
import {Settings} from "../shared/models/settings.interface";

@Component({
  selector: 'app-settings-form',
  templateUrl: './settings-form.component.html',
  styleUrls: ['./settings-form.component.scss']
})
export class SettingsFormComponent implements OnInit, OnDestroy {

  @Input('setData') setData$?: Subject<Settings>;
  @Output() saveData = new EventEmitter<Settings>();

  form = new FormGroup({
      'beacons': new FormArray<FormGroup<{
        'callsign': FormControl<string | null>,
        'symbol': FormControl<string | null>,
        'overlay': FormControl<string | null>
        'comment': FormControl<string | null>,
        'smart_beacon': FormGroup<{
          'active': FormControl<boolean | null>,
          'slowRate': FormControl<number | null>,
          'slowSpeed': FormControl<number | null>,
          'fastRate': FormControl<number | null>,
          'fastSpeed': FormControl<number | null>,
          'minTxDist': FormControl<number | null>,
          'minDeltaBeacon': FormControl<number | null>,
          'turnMinDeg': FormControl<number | null>,
          'turnSlope': FormControl<number | null>
        }>}>>([], [Validators.required]),
      'lora': new FormGroup({
        'frequency': new FormControl(433775000, [Validators.required, Validators.min(100000000), Validators.max(3000000000)]),
        'spreadingFactor': new FormControl(12, [Validators.required, Validators.min(7), Validators.max(12)]),
        'signalBandwidth': new FormControl(125000, [Validators.required, Validators.min(125000), Validators.max(500000)]),
        'codingRate4': new FormControl(5, [Validators.required, Validators.min(5), Validators.max(8)]),
        'power': new FormControl(20, [Validators.required, Validators.min(0), Validators.max(20)])
      }),
      'other': new FormGroup({
        'sendAltitude': new FormControl(true, [Validators.required]),
        'sendBatteryInfo': new FormControl(false, [Validators.required]),
        'showSymbolOnScreen': new FormControl(true, [Validators.required]),
        'displayEcoMode': new FormControl(true, [Validators.required]),
        'bluetooth': new FormControl(true, [Validators.required]),
        'disableGps': new FormControl(true, [Validators.required]),
        'simplifiedTrackerMode': new FormControl(false, [Validators.required]),
        'path': new FormControl('WIDE1-1', [Validators.required, Validators.minLength(1), Validators.maxLength(9)]),
        'sendCommentAfterXBeacons': new FormControl(10, [Validators.required, Validators.min(0), Validators.max(65535)]),
        'displayTimeout': new FormControl(60, [Validators.required, Validators.min(10), Validators.max(65535)]),
        'standingUpdateTime': new FormControl(15, [Validators.required, Validators.min(0), Validators.max(65535)]),
        'nonSmartBeaconRate': new FormControl(1, [Validators.required, Validators.min(1), Validators.max(65535)]),
        'rememberStationTime': new FormControl(30, [Validators.required, Validators.min(1), Validators.max(65535)]),
        'maxDistanceToTracker': new FormControl(30, [Validators.required, Validators.min(1), Validators.max(65535)]),
      }),
      'pttTrigger': new FormGroup({
        'active': new FormControl(false, [Validators.required]),
        'reverse': new FormControl(false, [Validators.required]),
        'io_pin': new FormControl(4, [Validators.required, Validators.min(0), Validators.max(30)]),
        'preDelay': new FormControl(0, [Validators.required, Validators.min(0), Validators.max(65535)]),
        'postDelay': new FormControl(0, [Validators.required, Validators.min(0), Validators.max(65535)]),
      }),
      'bme': new FormGroup({
        'active': new FormControl(false, [Validators.required]),
        'sendTelemetry': new FormControl(false, [Validators.required]),
        'heightCorrection': new FormControl(0, [Validators.required, Validators.min(0), Validators.max(65535)])
      }),
      'notification': new FormGroup({
        'ledTx': new FormControl(false, [Validators.required]),
        'ledTxPin': new FormControl(13, [Validators.required, Validators.min(0), Validators.max(40)]),
        'ledMessage': new FormControl(false, [Validators.required]),
        'ledMessagePin': new FormControl(2, [Validators.required, Validators.min(0), Validators.max(40)]),
        'buzzerActive': new FormControl(false, [Validators.required]),
        'buzzerPinTone': new FormControl(33, [Validators.required, Validators.min(0), Validators.max(40)]),
        'buzzerPinVcc': new FormControl(25, [Validators.required, Validators.min(0), Validators.max(40)]),
        'bootUpBeep': new FormControl(false, [Validators.required]),
        'txBeep': new FormControl(false, [Validators.required]),
        'messageRxBeep': new FormControl(false, [Validators.required]),
        'stationBeep': new FormControl(false, [Validators.required]),
        'lowBatteryBeep': new FormControl(false, [Validators.required])
      })
    }
  );

  private readonly subscriptions = new Subscription();

  public constructor(private readonly cookieService: CookieService,
                     private readonly toastService: NotifierService) {
  }

  ngOnInit(): void {
    this.addBeaconFormGroup();

    this.subscriptions.add(this.setData$?.subscribe(data => {
      this.setFormData(data);
    }));

    this.subscriptions.add(this.form.valueChanges
      .pipe(
        filter(() => this.form.valid),
        debounceTime(1000),
      )
      .subscribe(() => {
        this.cookieService.set('config', JSON.stringify(this.form.value));
      }))

    if (this.cookieService.check('config')) {
      const jsonData = JSON.parse(this.cookieService.get('config'));
      this.setFormData(jsonData);
      this.toastService.show({
        type: 'info',
        message: 'Configuration loaded from cache',
      });
    }
  }

  ngOnDestroy(): void {
    this.subscriptions.unsubscribe();
  }

  addBeaconFormGroup() {
    this.form.controls.beacons.push(new FormGroup({
      'callsign': new FormControl('NOCALL-7', [Validators.required, Validators.minLength(1), Validators.maxLength(9)]),
      'symbol': new FormControl('>', [Validators.required, Validators.minLength(1), Validators.maxLength(1)]),
      'overlay': new FormControl('/', [Validators.required, Validators.minLength(1), Validators.maxLength(1)]),
      'comment': new FormControl('', Validators.maxLength(40)),
      'smart_beacon': new FormGroup({
        'active': new FormControl(true, [Validators.required]),
        'slowRate': new FormControl(120, [Validators.required, Validators.min(0), Validators.max(65535)]),
        'slowSpeed': new FormControl(10, [Validators.required, Validators.min(0), Validators.max(65535)]),
        'fastRate': new FormControl(60, [Validators.required, Validators.min(0), Validators.max(65535)]),
        'fastSpeed': new FormControl(70, [Validators.required, Validators.min(0), Validators.max(65535)]),
        'minTxDist': new FormControl(100, [Validators.required, Validators.min(0), Validators.max(65535)]),
        'minDeltaBeacon': new FormControl(12, [Validators.required, Validators.min(0), Validators.max(65535)]),
        'turnMinDeg': new FormControl(10, [Validators.required, Validators.min(0), Validators.max(360)]),
        'turnSlope': new FormControl(80, [Validators.required, Validators.min(0), Validators.max(360)])
      })
    }));
  }

  removeBeacon(index: number): void {
    if (this.form.controls.beacons.at(index)) {
      this.form.controls.beacons.removeAt(index);
    }
  }

  submit(): void {
    this.saveData.next(this.form.value as Settings);
  }

  private setFormData(data: Settings): void {
    this.form.reset();

    while (this.form.controls.beacons.controls.length > 0) {
      this.removeBeacon(0);
    }

    for (let i = 0; i < data.beacons.length; i++) {
      this.addBeaconFormGroup();
    }

    this.form.patchValue(data);
  }
}
