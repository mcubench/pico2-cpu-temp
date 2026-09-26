// Diagnostic build: dumps the raw ADC state so we can see WHY channel 4
// is pegged at full scale. No load, no core1, one line per second.

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/structs/adc.h"

static uint16_t read_ch(uint ch)
{
    adc_select_input(ch);
    sleep_us(100);          // settle after mux change
    (void)adc_read();       // throw the first one away
    return adc_read();
}

int main(void)
{
    stdio_init_all();
    sleep_ms(3000);

    adc_init();
    adc_set_temp_sensor_enabled(true);
    sleep_ms(10);           // sensor bias start-up

    printf("# NUM_ADC_CHANNELS=%d  TEMP_CH=%d  clk_adc=%lu Hz\n",
           (int)NUM_ADC_CHANNELS, (int)ADC_TEMPERATURE_CHANNEL_NUM,
           (unsigned long)clock_get_hz(clk_adc));

    while (true) {
        // Re-assert the temperature sensor bias every loop, in case
        // something is clearing it.
        hw_set_bits(&adc_hw->cs, ADC_CS_TS_EN_BITS);
        sleep_ms(1);

        uint16_t ch4 = read_ch(ADC_TEMPERATURE_CHANNEL_NUM);
        uint32_t cs  = adc_hw->cs;

        float v = ch4 * (3.3f / 4096.0f);
        float t = 27.0f - (v - 0.706f) / 0.001721f;

        printf("cs=0x%08lx ts_en=%d ainsel=%lu ready=%d err=%d | ch4=%4u (%.3f V, %.2f C)"
               " | ch0=%4u ch1=%4u ch2=%4u ch3=%4u\n",
               (unsigned long)cs,
               (cs & ADC_CS_TS_EN_BITS) ? 1 : 0,
               (unsigned long)((cs & ADC_CS_AINSEL_BITS) >> ADC_CS_AINSEL_LSB),
               (cs & ADC_CS_READY_BITS) ? 1 : 0,
               (cs & ADC_CS_ERR_BITS) ? 1 : 0,
               ch4, v, t,
               read_ch(0), read_ch(1), read_ch(2), read_ch(3));

        sleep_ms(1000);
    }
}
