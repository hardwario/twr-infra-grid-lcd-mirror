#include <application.h>

// LED instance
bc_led_t led;

// Button instance
bc_button_t button;

bc_gfx_t *pgfx;

uint32_t shade = 0;


#define PCTL 0x00
#define RST 0x01
#define FPSC 0x02
#define INTC 0x03
#define STAT 0x04
#define SCLR 0x05
#define AVE 0x07
#define INTHL 0x08
#define TTHL 0x0E
#define TTHH 0x0F
#define INT0 0x10
#define T01L 0x80

#define AMG88_ADDR 0x68 // in 7bit

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_led_set_mode(&led, BC_LED_MODE_TOGGLE);
        shade++;

        if(shade == 4)
        {
            shade = 0;
        }
    }

    // Logging in action
    bc_log_info("Button event handler - event: %i", event);
}

void infra_init()
{
        // 10fps
    bc_i2c_memory_write_8b (BC_I2C_I2C0, AMG88_ADDR, FPSC, 0x00);
    // diff interrpt mode, INT output reactive
    bc_i2c_memory_write_8b (BC_I2C_I2C0, AMG88_ADDR, INTC, 0x00);
    // moving average output mode active
    bc_i2c_memory_write_8b (BC_I2C_I2C0, AMG88_ADDR, 0x1F, 0x50);
    bc_i2c_memory_write_8b (BC_I2C_I2C0, AMG88_ADDR, 0x1F, 0x45);
    bc_i2c_memory_write_8b (BC_I2C_I2C0, AMG88_ADDR, 0x1F, 0x57);
    bc_i2c_memory_write_8b (BC_I2C_I2C0, AMG88_ADDR, AVE, 0x20);
    bc_i2c_memory_write_8b (BC_I2C_I2C0, AMG88_ADDR, 0x1F, 0x00);

    //int sensorTemp[2];
    //dataread(AMG88_ADDR,TTHL,sensorTemp,2);

    int8_t temperature[2];
    bc_i2c_memory_read_8b(BC_I2C_I2C0, AMG88_ADDR, TTHL, (uint8_t*)&temperature[0]);
    bc_i2c_memory_read_8b(BC_I2C_I2C0, AMG88_ADDR, TTHH, (uint8_t*)&temperature[1]);

    //

    volatile float t = (temperature[1]*256 + temperature[0]) * 0.0625;
    t++;
}

void application_init(void)
{
    // Initialize logging
    bc_log_init(BC_LOG_LEVEL_DUMP, BC_LOG_TIMESTAMP_ABS);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_ON);

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    bc_module_lcd_init();
    pgfx = bc_module_lcd_get_gfx();

    bc_scheduler_disable_sleep();
    bc_system_pll_enable();

    infra_init();
}

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

int16_t sensorData[64];
float temperatures[64];

void infra_redraw()
{
            // read each 32 bytes
        //dataread(AMG88_ADDR, T01L + i*0x20, sensorData, 32);
        bc_i2c_memory_transfer_t transfer;
        transfer.device_address = AMG88_ADDR;
        transfer.memory_address = T01L; // + i*0x20;
        transfer.buffer = sensorData;
        transfer.length = 32*4;
        bc_i2c_memory_read(BC_I2C_I2C0, &transfer);

        float min_temperature = 20;
        float max_temperature = 24;

        for(int l = 0 ; l < 64 ; l++)
        {
            int16_t temporaryData = sensorData[l];
            float temperature;
            if (temporaryData > 0x200)
            {
                temperature = (-temporaryData +  0xfff) * -0.25;
            }
            else
            {
                temperature = temporaryData * 0.25;
            }

            if (temperature > max_temperature)
            {
                max_temperature = temperature;
            }

            temperatures[l] = temperature;
        }


        uint8_t row;
        uint8_t col;


        for (col = 0; col < 16; col++)
        {
            for (row = 0; row < 16; row++)
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

                uint16_t dithering_table[] = {0x0000, 0x1080, 0x1428, 0x7ebd, 0xffff};
                uint8_t r_row = 16 - row;

                bc_gfx_draw_fill_rectangle_dithering(pgfx, r_row * 8, col * 8, r_row * 8 + 8, col * 8 + 8, dithering_table[map_index]);

            }
        }
}

void application_task(void)
{

    if (bc_gfx_display_is_ready(pgfx))
    {

        /*bc_gfx_draw_fill_rectangle_dithering(pgfx, 0, 0, 128, 20, 0x1080);

        bc_gfx_draw_fill_rectangle_dithering(pgfx, 0, 20, 128, 40, 0x1428);

        bc_gfx_draw_fill_rectangle_dithering(pgfx, 0, 40, 128, 60, 0x7ebd);

        bc_gfx_draw_fill_rectangle_dithering(pgfx, 0, 60, 128, 80, 0xffff); //0x7BDE

        bc_gfx_draw_fill_rectangle_dithering(pgfx, 0, 80, 128, 100, 0xa5a5);

        bc_gfx_draw_fill_rectangle_dithering(pgfx, 0, 100, 128, 128, 0xf5f5);*/

        infra_redraw();

        bc_gfx_update(pgfx);


    }

    // Plan next run this function after 1000 ms
    bc_scheduler_plan_current_relative(20);
}



void application_task2(void)
{
    static uint32_t counter = 0;

    if (bc_gfx_display_is_ready(pgfx))
    {

        bc_tick_t start_tick = bc_tick_get();

        if (shade > counter)
        {
            bc_gfx_draw_fill_rectangle(pgfx, 10, 10, 20, 20, 1);
        }
        else
        {
            bc_gfx_draw_fill_rectangle(pgfx, 10, 10, 20, 20, 0);
        }

        bc_gfx_update(pgfx);

        counter++;
        if(counter == 4)
        {
            counter = 0;
        }

        volatile bc_tick_t diff = bc_tick_get() - start_tick;
        diff++;

    }

    // Plan next run this function after 1000 ms
    bc_scheduler_plan_current_now();
}
