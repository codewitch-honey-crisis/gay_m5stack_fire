#include <Arduino.h>
#include <SPIFFS.h>
#include <SD.h>
#include <mpu6886.hpp>
#include <ili9341.hpp>
#include <tft_io.hpp>
#include <htcw_button.hpp>
#include <gfx.hpp>
#include <w2812.hpp>
#include "Robinette.hpp"
using namespace arduino;
using namespace gfx;

constexpr static const uint8_t spi_host = VSPI;
constexpr static const int8_t lcd_pin_bl = 32;
constexpr static const int8_t lcd_pin_dc = 27;
constexpr static const int8_t lcd_pin_rst = 33;
constexpr static const int8_t lcd_pin_cs = 14;
constexpr static const int8_t sd_pin_cs = 4;
constexpr static const int8_t speaker_pin_cs = 25;
constexpr static const int8_t mic_pin_cs = 34;
constexpr static const int8_t button_a_pin = 39;
constexpr static const int8_t button_b_pin = 38;
constexpr static const int8_t button_c_pin = 37;
constexpr static const int8_t led_pin = 15;
constexpr static const int8_t spi_pin_mosi = 23;
constexpr static const int8_t spi_pin_clk = 18;
constexpr static const int8_t spi_pin_miso = 19;

using bus_t = tft_spi_ex<spi_host, 
                        lcd_pin_cs, 
                        spi_pin_mosi, 
                        spi_pin_miso, 
                        spi_pin_clk, 
                        SPI_MODE0,
                        false, 
                        320 * 240 * 2 + 8, 2>;
// can be simplified since the M5 Stack
// uses the default VSPI pins:
// using bus_t = tft_spi<spi_host,lcd_pin_cs,SPI_MODE0,320*240*2+8,2>;

using lcd_t = ili9342c<lcd_pin_dc, 
                      lcd_pin_rst, 
                      lcd_pin_bl, 
                      bus_t, 
                      1, 
                      true, 
                      400, 
                      200>;

using color_t = color<typename lcd_t::pixel_type>;

lcd_t lcd;

// declare the MPU6886 that's attached
// to the first I2C host
mpu6886 gyro(i2c_container<0>::instance());
// the following is equiv at least on the ESP32
// mpu6886 mpu(Wire);

w2812 led_strips(10,led_pin,NEO_GBR);

button<button_a_pin,10,true> button_a;
button<button_b_pin,10,true> button_b;
button<button_c_pin,10,true> button_c;

// initialize M5 Stack Fire peripherals/features
void initialize_m5stack_fire() {
    Serial.begin(115200);
    SPIFFS.begin(false);
    SD.begin(4,spi_container<spi_host>::instance());
    // since the peripherals share a bus, 
    // make sure the first one (the LCD)
    // with the pin assignments is 
    // initialized first.
    lcd.initialize();
    lcd.fill(lcd.bounds(),color_t::purple);
    rect16 rect(0,0,64,64);
    rect.center_inplace(lcd.bounds());
    lcd.fill(rect,color_t::white);
    lcd.fill(rect.inflate(-8,-8),color_t::purple);
    //gyro.initialize();
    // see https://github.com/m5stack/m5-docs/blob/master/docs/en/core/fire.md
    pinMode(led_pin, OUTPUT_OPEN_DRAIN);
    led_strips.initialize();
    button_a.initialize();
    button_b.initialize();
    button_c.initialize();
    lcd.fill(lcd.bounds(),color_t::black);
}
const char* text = "gay!";
constexpr static const uint16_t text_height = 200;
srect16 text_rect;
open_text_info text_draw_info;
const open_font &text_font = Robinette;
const rgb_pixel<16> colors[] = {
    color_t::red,
    color_t::orange,
    color_t::yellow,
    color_t::green,
    color_t::blue,
    color_t::purple
};
constexpr const size_t color_count = sizeof(colors)/sizeof(rgb_pixel<16>);
const int color_height = lcd.dimensions().height/color_count;
unsigned int color_offset;
uint32_t led_strip_ts;
uint32_t led_strip_offset;

using frame_buffer_t = bitmap_type_from<lcd_t>;
uint8_t* frame_buffer_data;
frame_buffer_t frame_buffer;
void setup() {
    initialize_m5stack_fire();
    // your code here
    led_strip_ts=0;
    led_strip_offset = 0;
    color_offset = 0;
    frame_buffer_data = (uint8_t*)ps_malloc(frame_buffer_t::sizeof_buffer(lcd.dimensions()));
    if(frame_buffer_data==nullptr) {
        Serial.println("Out of memory.");
        while(true) {delay(10000);}
    }
    frame_buffer = create_bitmap_from(lcd,lcd.dimensions(),frame_buffer_data);
    rect16 r(0,0,lcd.bounds().x2,color_height-1);
    frame_buffer.fill(r,colors[0]);
    for(size_t i = 1;i<color_count;++i) {
        r.offset_inplace(0,color_height);
        frame_buffer.fill(r,colors[i]);
    }
    draw::bitmap(lcd,lcd.bounds(),frame_buffer,frame_buffer.bounds());
    text_draw_info.text = text;
    text_draw_info.font = &text_font;
    text_draw_info.scale = text_font.scale(text_height);
    text_rect = text_font.measure_text(ssize16::max(),spoint16::zero(),text,text_draw_info.scale).bounds().center((srect16)lcd.bounds());
}
void loop() {  
    rect16 r(0,(color_height-1+color_offset)%240,lcd.bounds().x2,(color_height-1+color_offset)%240);
    frame_buffer.fill(r,colors[0]);
    for(size_t i = 1;i<color_count;++i) {
        r.offset_inplace(0,color_height);
        r.y1=r.y2=(r.y1%240);
        frame_buffer.fill(r,colors[i]);
    }
    draw::text(frame_buffer,text_rect,text_draw_info,color_t::black);
    draw::bitmap(lcd,lcd.bounds(),frame_buffer,frame_buffer.bounds());   
    uint32_t ms = millis();
    if(ms>=led_strip_ts+250) {
        led_strip_ts = ms;
        draw::suspend(led_strips);
        for(int x = 0;x<led_strips.dimensions().width;++x) {
            draw::point(led_strips,point16(x,0),colors[(led_strip_offset+x)%color_count]);
        }
        draw::resume(led_strips);
        ++led_strip_offset;
    }
    ++color_offset;
    
}