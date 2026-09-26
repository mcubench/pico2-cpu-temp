// Minimal RP2350A (Pico 2) core temperature monitor with synthetic load.
//
// Conversion follows the RP2350 datasheet (same formula Zephyr's
// rpi_pico_temp driver uses): T = 27 - (Vbe - 0.706) / 0.001721
//
// Output: one CSV line per second on stdio (USB CDC and UART0).

#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/adc.h"
#include "hardware/structs/adc.h"

#define LOAD_PERIOD_S   30      // 30 s full load, 30 s idle, repeating
#define ADC_SAMPLES     64      // oversampling per reading
#define ADC_VREF        3.3f
#define ADC_RANGE       (1 << 12)

// Datasheet constants (RP2350 §12.4.6 / RP2040 §4.9.5 - identical)
#define VBE_27C         0.706f
#define VBE_SLOPE       0.001721f

// Temperature sensor ADC channel. The SDK derives this from the board
// header (4 on RP2040/RP2350A, 8 on RP2350B); fall back for old SDKs.
#ifndef ADC_TEMPSENSOR_INPUT
#ifdef ADC_TEMPERATURE_CHANNEL_NUM
#define ADC_TEMPSENSOR_INPUT ADC_TEMPERATURE_CHANNEL_NUM
#else
#define ADC_TEMPSENSOR_INPUT 4
#endif
#endif

static volatile bool load_on = false;

// Keeps the pipeline and the FPU busy; volatile stops the optimizer
// from throwing the whole thing away.
static void burn(void)
{
    volatile float x = 1.234f;
    for (int i = 0; i < 20000; i++) {
        x = x * 1.0001f + 0.5f / (x + 1.0f);
    }
}

static void core1_main(void)
{
    while (true) {
        if (load_on) {
            burn();
        } else {
            sleep_ms(5);
        }
    }
}

static float read_temp_c(void)
{
    uint32_t acc = 0;

    // Make sure the sensor bias is still on, then let the mux settle and
    // throw away the first conversion after the input change.
    hw_set_bits(&adc_hw->cs, ADC_CS_TS_EN_BITS);
    adc_select_input(ADC_TEMPSENSOR_INPUT);
    sleep_us(200);
    (void)adc_read();

    for (int i = 0; i < ADC_SAMPLES; i++) {
        acc += adc_read();
    }

    float raw = (float)acc / (float)ADC_SAMPLES;
    float vbe = raw * (ADC_VREF / (float)ADC_RANGE);

    return 27.0f - (vbe - VBE_27C) / VBE_SLOPE;
}

int main(void)
{
    stdio_init_all();

    adc_init();
    adc_set_temp_sensor_enabled(true);
    sleep_ms(2000);                 // USB enumeration + sensor warm-up

    multicore_launch_core1(core1_main);

    printf("# second,state,temp_c\n");

    absolute_time_t next = get_absolute_time();
    uint32_t t = 0;

    while (true) {
        load_on = ((t / LOAD_PERIOD_S) % 2) == 0;

        float temp = read_temp_c();
        printf("%lu,%s,%.2f\n", (unsigned long)t, load_on ? "LOAD" : "IDLE", temp);

        // Spend the rest of the second either burning cycles or sleeping.
        next = delayed_by_ms(next, 1000);
        while (absolute_time_diff_us(get_absolute_time(), next) > 0) {
            if (load_on) {
                burn();
            } else {
                sleep_ms(1);
            }
        }
        t++;
    }
}
