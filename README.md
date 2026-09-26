# pico2-cpu-temp
Minimal RP2350 firmware that reads the on-die temperature sensor once per second
and prints CSV over USB/UART, while alternating 30 s of synthetic full-core load
with 30 s idle to make the temperature swing.

## Build

```sh
export PICO_SDK_PATH=/path/to/pico-sdk   # 2.x
mkdir build && cd build
cmake .. && make -j
```

Hold BOOTSEL, plug in, copy `build/pico2-cpu-temp.uf2` or `build/pico2-cpu-temp_diag.uf2` to the drive.

## Output

```
# second,state,temp_c
0,LOAD,24.87
30,IDLE,31.10
```

115200 baud on `/dev/ttyACM0` (or the board's COM port).

## Files

| File | Purpose |
|---|---|
| `main.c` | Temperature monitor + load cycle |
| `diag.c` | Diagnostic build: dumps ADC `CS` register and all channels |

## How it works

- Temperature sensor is ADC channel 4 on RP2040/RP2350A/RP2354A, channel 8 on
  RP2350B. Use the SDK's `ADC_TEMPERATURE_CHANNEL_NUM` rather than hardcoding it.
- `T = 27 - (Vbe - 0.706) / 0.001721`, `Vbe = raw * 3.3 / 4096` (RP2350 datasheet,
  same formula as Zephyr's `rpi_pico_temp` driver).
- 64-sample oversampling; first conversion after a mux change is discarded.
- Core1 runs the same float-heavy loop as core0 so both cores heat up.

## Notes from debugging

- A constant reading of `-1479.80 °C` is raw ADC 4095 — the conversion is fine,
  the ADC is pegged at the rail. Don't debug the math.
- `diag.c` decodes `ADC.CS`. If `ERR`/`ERR_STICKY` are set **and every channel
  reads 4095**, the fault is analog, not software: check that `ADC_AVDD` and
  `ADC_VREF` are actually supplied on your board.
- Third-party RP2350/RP2354 boards often leave those pins unpopulated or
  floating. A genuine Pico 2 filters them from 3V3.
- RP2354A is an RP2350A with 2 MB in-package flash — identical ADC, but the
  `pico2` board header declares the wrong flash size. Use the vendor's header.
- RP2350 ADC accuracy is roughly ±2 °C absolute (INL/DNL); relative swing under
  load is what this project measures, and that is reliable.

