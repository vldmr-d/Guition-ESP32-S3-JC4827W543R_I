#include <lvgl.h>
#include <Arduino_GFX_Library.h>

//#if LV_BUILD_EXAMPLES &&  LV_USE_DEMO_BENCHMARK && LV_USE_DEMO_WIDGETS

//#include <demos/lv_demos.h>
//#include <demos/widgets/lv_demo_widgets.h>

//#endif

//#define GFX_BL DF_GFX_BL // default backlight pin, you may replace DF_GFX_BL to actual backlight pin
#define GFX_BL 1

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    45 /* cs */, 47 /* sck */, 21 /* d0 */, 48 /* d1 */, 40 /* d2 */, 39 /* d3 */);
Arduino_GFX *g = new Arduino_NV3041A(bus, GFX_NOT_DEFINED /* RST */, 0 /* rotation */, true /* IPS */);
Arduino_GFX *gfx = new Arduino_Canvas(480 /* width */, 272 /* height */, g);

#define CANVAS

/*******************************************************************************
 * End of Arduino_GFX setting
 ******************************************************************************/

/*******************************************************************************
 * Please config the touch panel in touch.h
 ******************************************************************************/
#include "touch.h"

/* Change to your screen resolution */
uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;
lv_display_t *disp;
lv_color_t *disp_draw_buf;

#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char *buf)
{
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}
#endif

uint32_t millis_cb(void)
{
  return millis();
}

/* LVGL calls it when a rendered image needs to copied to the display*/
/* Display flushing */
void my_disp_flush(lv_disp_t *disp, const lv_area_t *area, uint8_t * px_map)
{
#ifndef DIRECT_MODE
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);

  #if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t*)full_color, w, h);
  #else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)px_map, w, h);
  #endif
  #endif // #ifndef DIRECT_MODE

  lv_disp_flush_ready(disp);
}


void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data)
{
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PR;

      /*Set the coordinates*/
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
    }
    else if (touch_released())
    {
      data->state = LV_INDEV_STATE_REL;
    }
  }
  else
  {
    data->state = LV_INDEV_STATE_REL;
  }
}
 

void setup()
{
  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  // while(!Serial);

  Serial.println("Arduino_GFX LVGL_Arduino example v9");

  String LVGL_Arduino = "Hello Arduino! ";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println( LVGL_Arduino );

  #ifdef GFX_EXTRA_PRE_INIT
    GFX_EXTRA_PRE_INIT();
  #endif

  // Init Display
  if (!gfx->begin())
  {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(BLACK);


  #ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
  #endif

  Touch_SPI.begin(TOUCH_XPT2046_SCK, TOUCH_XPT2046_MISO, TOUCH_XPT2046_MOSI, TOUCH_XPT2046_CS);
  ts.begin(Touch_SPI);
  ts.setRotation(TOUCH_XPT2046_ROTATION);
  
  // Init touch device
  touch_init(gfx->width(), gfx->height(), gfx->getRotation());


  lv_init();

  /*Set a tick source so that LVGL will know how much time elapsed. */
  lv_tick_set_cb(millis_cb);

  /* register print function for debugging */
  #if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
  #endif

  screenWidth = gfx->width();
  screenHeight = gfx->height();

  #ifdef DIRECT_MODE
    bufSize = screenWidth * screenHeight;
  #else
    bufSize = screenWidth * 40;
  #endif

  #ifdef ESP32
  #if defined(DIRECT_MODE) && defined(RGB_PANEL)
    disp_draw_buf = (lv_color_t *)gfx->getFramebuffer();
  #else  // !DIRECT_MODE
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!disp_draw_buf)
    {
      // remove MALLOC_CAP_INTERNAL flag try again
      disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_8BIT);
    }
  #endif // !DIRECT_MODE
  #else  // !ESP32
    Serial.println("LVGL draw_buf allocate MALLOC_CAP_INTERNAL failed! malloc again...");
    disp_draw_buf = (lv_color_t *)malloc(bufSize * 2);
  #endif // !ESP32
    if (!disp_draw_buf)
    {
      Serial.println("LVGL disp_draw_buf allocate failed!");
    }
    else
    {
      disp = lv_display_create(screenWidth, screenHeight);
      lv_display_set_flush_cb(disp, my_disp_flush);
  #ifdef DIRECT_MODE
      lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_DIRECT);
  #else
      lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
  #endif

    /*Initialize the (dummy) input device driver*/
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /*Touchpad should have POINTER type*/
    lv_indev_set_read_cb(indev, my_touchpad_read);

    /* Option 1: Create a simple label
      * ---------------------
      */
      lv_obj_t *label = lv_label_create(lv_scr_act());
      lv_label_set_text(label, "Hello Arduino, I'm LVGL!(V" GFX_STR(LVGL_VERSION_MAJOR) "." GFX_STR(LVGL_VERSION_MINOR) "." GFX_STR(LVGL_VERSION_PATCH) ")");
      lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    /* Create a simple label
      * ---------------------
      lv_obj_t *label = lv_label_create( lv_screen_active() );
      lv_label_set_text( label, "Hello Arduino, I'm LVGL!" );
      lv_obj_align( label, LV_ALIGN_CENTER, 0, 0 );

      * Try an example. See all the examples
      *  - Online: https://docs.lvgl.io/master/examples.html
      *  - Source codes: https://github.com/lvgl/lvgl/tree/master/examples
      * ----------------------------------------------------------------

      lv_example_btn_1();

      * Or try out a demo. Don't forget to enable the demos in lv_conf.h. E.g. LV_USE_DEMOS_WIDGETS
      * -------------------------------------------------------------------------------------------

      lv_demo_widgets();
      */
    //lv_demo_widgets();
    //lv_demo_keypad_encoder();
    //lv_demo_benchmark();
  }


    Serial.printf("w %d" , lv_display_get_horizontal_resolution(disp));
    Serial.printf(", h %d \n" , lv_display_get_vertical_resolution(disp));
    Serial.printf("DPI %d \n", lv_display_get_dpi(disp));

    Serial.println("Setup done");
  
}

void loop()
{ 
  lv_timer_handler(); /* let the GUI do its work */

  #ifdef DIRECT_MODE
  #if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(0, 0, (uint16_t *)disp_draw_buf, screenWidth, screenHeight);
  #else
    gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)disp_draw_buf, screenWidth, screenHeight);
  #endif
  #endif // #ifdef DIRECT_MODE

  #ifdef CANVAS
    gfx->flush();
  #endif

  delay(5);
}