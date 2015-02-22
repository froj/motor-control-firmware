#include <ch.h>
#include <hal.h>
#include <filter/iir.h>

#define ADC_MAX         4096
#define ADC_TO_AMPS     0.001611328125f // 3.3/4096/(0.01*50)
#define ADC_TO_VOLTS    0.005281575521f // 3.3/4096/(18/(100+18))

static float motor_current;
static float battery_voltage;
static float aux_in;

float analog_get_battery_voltage(void)
{
    return battery_voltage;
}

float analog_get_motor_current(void)
{
    return motor_current;
}

float analog_get_auxiliary(void)
{
    return aux_in;
}

static void adc_callback(ADCDriver *adcp, adcsample_t *adc_samples, size_t n)
{
    (void)adcp;
    (void)n;

    static filter_iir_t current_filter;
    // scipy.signal.butter(3, 0.1)


    static const float b[] = {0.01, 0.0};
    static const float a[] = {-0.99};

    static float current_filter_buffer[1];

    static bool initialized = false;

    if (!initialized) {
        filter_iir_init(&current_filter, b, a, 1, current_filter_buffer);
        initialized = true;
    }

    motor_current = filter_iir_apply(&current_filter,
            (- (adc_samples[1] - ADC_MAX / 2) * ADC_TO_AMPS));

    battery_voltage = adc_samples[3] * ADC_TO_VOLTS;
    aux_in = (float)(adc_samples[0] + adc_samples[2])/(ADC_MAX*2);
}

static THD_FUNCTION(adc_task, arg)
{
    (void)arg;
    chRegSetThreadName("adc read");
    static adcsample_t adc_samples[4];
    static const ADCConversionGroup adcgrpcfg1 = {
        TRUE,                   // circular?
        4,                      // nb channels
        adc_callback,           // callback fn
        NULL,                   // error callback fn
        ADC_CFGR_CONT,          // CFGR : continuous mode
        0,                      // TR1
        6,                      // CCR : DUAL=regualar,simultaneous
        {ADC_SMPR1_SMP_AN1(0), 0},                          // SMPRx : sample time minimum
        {ADC_SQR1_NUM_CH(2) | ADC_SQR1_SQ1_N(1) | ADC_SQR1_SQ2_N(1), 0, 0, 0}, // SQRx : ADC1_CH1 2x
        {ADC_SMPR1_SMP_AN2(0) | ADC_SMPR1_SMP_AN3(0), 0},   // SSMPRx : sample time minimum
        {ADC_SQR1_NUM_CH(2) | ADC_SQR1_SQ1_N(2) | ADC_SQR1_SQ2_N(3), 0, 0, 0}, // SSQRx : ADC2_CH2, ADC2_CH3
    };


    adcStart(&ADCD1, NULL);

    adcConvert(&ADCD1, &adcgrpcfg1, adc_samples, 1); // should never return

    return 0;
}

void analog_init(void)
{
    static THD_WORKING_AREA(adc_task_wa, 256);
    chThdCreateStatic(adc_task_wa, sizeof(adc_task_wa), HIGHPRIO, adc_task, NULL);
}
