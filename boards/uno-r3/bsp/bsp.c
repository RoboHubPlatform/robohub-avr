/*
 * bsp.c — Support package for the Arduino UNO R3.
 *
 * The board's facts: which pin the LED is on, which pins the buses are on, and
 * at what speed the console runs. A program asks for a name — "user_led",
 * "system_timer", "i2c0" — and never for a pin, so the same program runs on a
 * board with a different pinout.
 *
 * The board registers its peripherals in `robohub_board_register`, which the
 * platform's own bring-up calls: the platform knows the part and the board knows
 * the pins, and neither has to know the other.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "bsp.h"

#include "registry.h"

#include "robohub/hal/console.h"
#include "robohub/platform/avr_platform.h"
#include "robohub/platform/avr_gpio.h"
#include "robohub/platform/avr_uart.h"
#include "robohub/platform/avr_timer.h"

#if defined(CONFIG_RH_HAL_ONEWIRE) && CONFIG_RH_HAL_ONEWIRE
#include "robohub/platform/avr_onewire.h"
#endif
#if defined(CONFIG_RH_HAL_DHT) && CONFIG_RH_HAL_DHT
#include "robohub/platform/avr_dht.h"
#endif
#if defined(CONFIG_RH_HAL_ADC) && CONFIG_RH_HAL_ADC
#include "robohub/platform/avr_adc.h"
#endif
#if defined(CONFIG_RH_HAL_PWM) && CONFIG_RH_HAL_PWM
#include "robohub/platform/avr_pwm.h"
#endif
#if defined(CONFIG_RH_HAL_I2C) && CONFIG_RH_HAL_I2C
#include "robohub/platform/avr_i2c.h"
#endif
#if defined(CONFIG_RH_HAL_SPI) && CONFIG_RH_HAL_SPI
#include "robohub/platform/avr_spi.h"
#endif

/* -------------------------------------------------------------------------- */
/*  Instances                                                                 */
/* -------------------------------------------------------------------------- */

/* The LED and the general pin line are the same interface on different pins, so
 * they are two instances of it rather than one instance with a table. */
static RoboHubAVRGPIO led_gpio;
static RoboHubAVRGPIO gpio0;

/* The console is the part's only USART. */
static RoboHubAVRUART uart0;

/* The clock every driver's delay is counted in. */
static RoboHubAVRTimer system_timer;

#if defined(CONFIG_RH_HAL_ONEWIRE) && CONFIG_RH_HAL_ONEWIRE
static RoboHubAVROneWire onewire0;
#endif
#if defined(CONFIG_RH_HAL_DHT) && CONFIG_RH_HAL_DHT
static RoboHubAVRDHT dht0;
#endif
#if defined(CONFIG_RH_HAL_ADC) && CONFIG_RH_HAL_ADC
static RoboHubAVRADC adc0;
#endif
#if defined(CONFIG_RH_HAL_PWM) && CONFIG_RH_HAL_PWM
static RoboHubAVRPWM pwm0;
#endif
#if defined(CONFIG_RH_HAL_I2C) && CONFIG_RH_HAL_I2C
static RoboHubAVRI2C i2c0;
#endif
#if defined(CONFIG_RH_HAL_SPI) && CONFIG_RH_HAL_SPI
static RoboHubAVRSPI spi0;
#endif

/* -------------------------------------------------------------------------- */
/*  Board init                                                                */
/* -------------------------------------------------------------------------- */

void bsp_init(void)
{
    /* The LED is an output from the start, so the board does not sit with its
     * pin floating while the program is still starting. */
    (void)avr_gpio_create(&led_gpio, "user_led", RH_UNO_R3_LED_PIN);
    (void)led_gpio.interface.set_mode(&led_gpio.interface, RH_UNO_R3_LED_PIN,
                                       ROBOHUB_GPIO_MODE_OUTPUT);
    (void)led_gpio.interface.write(&led_gpio.interface, RH_UNO_R3_LED_PIN, 0u);
}

void bsp_register_hals(void)
{
    (void)avr_gpio_create(&gpio0, "gpio0", 0u);
    (void)avr_uart_create(&uart0, "uart0");
    (void)avr_timer_create(&system_timer, "system_timer");

    robohub_register_gpio("user_led", &led_gpio.interface);
    robohub_register_gpio("gpio0", &gpio0.interface);
    robohub_register_uart("uart0", &uart0.interface);
    robohub_register_timer("system_timer", &system_timer.interface);

#if defined(CONFIG_RH_HAL_ONEWIRE) && CONFIG_RH_HAL_ONEWIRE
    (void)avr_onewire_create(&onewire0, "onewire0", RH_UNO_R3_ONEWIRE_PIN);
    robohub_register_onewire("onewire0", &onewire0.interface);
#endif

#if defined(CONFIG_RH_HAL_DHT) && CONFIG_RH_HAL_DHT
    (void)avr_dht_create(&dht0, "dht0", RH_UNO_R3_DHT_PIN);
    robohub_register_dht("dht0", &dht0.interface);
#endif

#if defined(CONFIG_RH_HAL_ADC) && CONFIG_RH_HAL_ADC
    (void)avr_adc_create(&adc0, "adc0");
    robohub_register_adc("adc0", &adc0.interface);
#endif

#if defined(CONFIG_RH_HAL_PWM) && CONFIG_RH_HAL_PWM
    (void)avr_pwm_create(&pwm0, "pwm0");
    robohub_register_pwm("pwm0", &pwm0.interface);
#endif

#if defined(CONFIG_RH_HAL_I2C) && CONFIG_RH_HAL_I2C
    (void)avr_i2c_create(&i2c0, "i2c0");
    robohub_register_i2c("i2c0", &i2c0.interface);
#endif

#if defined(CONFIG_RH_HAL_SPI) && CONFIG_RH_HAL_SPI
    (void)avr_spi_create(&spi0, "spi0");
    robohub_register_spi("spi0", &spi0.interface);
#endif
}

/* -------------------------------------------------------------------------- */
/*  Registration                                                              */
/* -------------------------------------------------------------------------- */

RoboHubError robohub_board_register(void)
{
    /* The board's own peripherals are created and registered first, because they
     * are what the platform is then brought up around: the console is the
     * USART, and a USART that has not been created has no operations to call
     * yet. */
    bsp_init();
    bsp_register_hals();

    /* The console is the USART, and the board is what knows it. */
    (void)avr_platform_init(&uart0);

    if (robohub_console_init() != ROBOHUB_CONSOLE_OK) {
        return ROBOHUB_ERR_IO;
    }
    (void)uart0.interface.init(&uart0.interface, RH_UNO_R3_CONSOLE_BAUD);

    return ROBOHUB_OK;
}
