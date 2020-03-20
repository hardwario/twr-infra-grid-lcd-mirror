#include <application.h>

// LED instance
bc_led_t led;

// Button instance
bc_button_t button;

// Graphics GFX instance
bc_gfx_t *pgfx;

// Infra Grid Module
bc_module_infra_grid_t infra;

bc_led_t lcd_led_red;
bc_led_t lcd_led_green;

// 8x8 temperature array
float temperatures[64];

bool display_temperature = true;
float temperature_level_value = 32.0f;
uint32_t temperature_level_timeout;

int32_t map_c(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max)
{
    int32_t val = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;

    if (val > out_max)
    {
        val = out_max;
    }
    if (val < out_min)
    {
        val = out_min;
    }

    return val;
}

void lcd_event_handler(bc_module_lcd_event_t event, void *param)
{
    (void) param;

    if (event == BC_MODULE_LCD_EVENT_LEFT_CLICK)
    {
        temperature_level_value -= 0.2;
        temperature_level_timeout = bc_tick_get() + 1500;
    }
    else if (event == BC_MODULE_LCD_EVENT_RIGHT_CLICK)
    {
        temperature_level_value += 0.2;
        temperature_level_timeout = bc_tick_get() + 1500;
    }
    else if (event == BC_MODULE_LCD_EVENT_BOTH_HOLD)
    {
        display_temperature = !display_temperature;
    }

}

void infra_handler(bc_module_infra_grid_t *self, bc_module_infra_grid_event_t event, void *param)
{
    if (event == BC_MODULE_INFRA_GRID_EVENT_ERROR)
    {
        volatile int i = 5;
        i++;
    }

    //return;

    if (event == BC_MODULE_INFRA_GRID_EVENT_UPDATE)
    {
        if (!bc_gfx_display_is_ready(pgfx))
        {
            return;
        }

        // Enable PLL to increase LCD redraw speed
        bc_system_pll_enable();

        bc_module_infra_grid_get_temperatures_celsius(self, temperatures);
        
        float min_temperature = 20;
        float max_temperature = 24;

        // Find max temperature
        for (int i = 0 ; i < 64 ; i++)
        {
            if (temperatures[i] > max_temperature)
            {
                max_temperature = temperatures[i];
            }
        }

        for (uint8_t col = 0; col < 16; col++)
        {
            for (uint8_t row = 0; row < 16; row++)
            {
                uint8_t map_index;
                uint32_t index_temp = (row/2) + (col/2) * 8;
                float temperature;

                if (((col % 2) == 0) && ((row % 2) == 0))
                {
                    temperature = temperatures[index_temp];
                }
                else if (((row % 2) == 1) && ((col % 2) == 0))
                {
                    temperature = (temperatures[index_temp] + temperatures[index_temp + 1]) / 2;
                }
                 else
                {
                    temperature = (temperatures[row / 2 + (col/2)*8] + temperatures[row / 2 + ((col+1)/2)*8]) / 2;
                }

                map_index = map_c(((uint32_t)temperature), min_temperature, max_temperature, 0, 4);

                const uint16_t dithering_table[] = {0x0000, 0x1080, 0x1428, 0x7ebd, 0xffff};
                uint8_t r_row = 16 - row;

                bc_gfx_draw_fill_rectangle_dithering(pgfx, r_row * 8, col * 8, r_row * 8 + 8, col * 8 + 8, dithering_table[map_index]);
            }
        }

        if (display_temperature)
        {
            char str_temp[10];
            float central_temperature = temperatures[4 + 4*8];
            //central_temperature = 38.6;
            snprintf(str_temp, sizeof(str_temp),"%.1f°C", central_temperature);
            bc_gfx_set_font(pgfx, &bc_font_ubuntu_13);
            int width = bc_gfx_calc_string_width(pgfx, str_temp);
            bc_gfx_draw_fill_rectangle(pgfx, 50, 50, 50 + width, 50 + 13, false);
            bc_gfx_draw_string(pgfx, 50, 50, str_temp, true);

            // Temperature set value
            if (temperature_level_timeout > bc_tick_get())
            {
                bc_gfx_printf(pgfx, 10, 0, true, "%0.1f", temperature_level_value);
            }

            if (central_temperature < temperature_level_value)
            {
                bc_led_set_mode(&lcd_led_green, BC_LED_MODE_ON);
                bc_led_set_mode(&lcd_led_red  , BC_LED_MODE_OFF);
            }
            else
            {
                bc_led_set_mode(&lcd_led_green, BC_LED_MODE_OFF);
                bc_led_set_mode(&lcd_led_red  , BC_LED_MODE_BLINK_FAST);
            }
        }

        bc_gfx_update(pgfx);

        bc_system_pll_disable();
    }
}

void application_init(void)
{
    // Initialize logging
    //bc_log_init(BC_LOG_LEVEL_DUMP, BC_LOG_TIMESTAMP_ABS);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_OFF);
    bc_led_pulse(&led, 1000);

    // Initialize button
    //bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    //bc_button_set_event_handler(&button, button_event_handler, NULL);

    // LCD Module
    bc_module_lcd_init();
    bc_module_lcd_set_event_handler(lcd_event_handler, NULL);
    pgfx = bc_module_lcd_get_gfx();
    // LEDs on LCD Module
    const bc_led_driver_t* driver = bc_module_lcd_get_led_driver();
    bc_led_init_virtual(&lcd_led_red, 0, driver, 1);
    bc_led_init_virtual(&lcd_led_green, 1, driver, 1);

    // Infra Grid Module
    bc_module_infra_grid_init(&infra);
    bc_module_infra_grid_set_event_handler(&infra, infra_handler, NULL);
    //bc_module_infra_grid_set_update_interval(&infra, 100);
}


void application_task()
{
    bc_module_infra_grid_measure(&infra);
    bc_scheduler_plan_current_relative(100);
}