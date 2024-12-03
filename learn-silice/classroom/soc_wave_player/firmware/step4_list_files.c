// @sylefeb 2022-01-10
// MIT license, see LICENSE_MIT in Silice repo root
// https://github.com/sylefeb/Silice/

#include "config.h"
#include "std.h"
#include "oled.h"
#include "display.h"
#include "printf.h"
#include "sdcard.h"
#define SIZE_ITEMS 100
#define SIZE_LEN 10

// include the fat32 library
#include "fat_io_lib/src/fat_filelib.h"

const char *item[SIZE_ITEMS][SIZE_LEN];

void main()
{
  // turn LEDs off
  *LEDS = 0;
  // install putchar handler for printf
  f_putchar = display_putchar;
  // init screen
  oled_init();
  oled_fullscreen();
  oled_clear(0);
  int selected = 0;
  int pulse = 0;
  int n_items = 0;
  // init sdcard
  sdcard_init();
  // initialise File IO Library
  fl_init();
  // attach media access functions to library
  while (fl_attach_media(sdcard_readsector, sdcard_writesector) != FAT_INIT_OK) {
    // keep trying, we need this
  }
  // header
  display_set_cursor(0,0);
  display_set_front_back_color(0,255);
  printf("    ===== files =====    \n\n");
  display_refresh();
  display_set_front_back_color(255,0);
  // list files (see fl_listdirectory if at_io_lib/src/fat_filelib.c)
  const char *path = "/";
  FL_DIR dirstat;
  // FL_LOCK(&_fs);
  if (fl_opendir(path, &dirstat)) {
    struct fs_dir_ent dirent;
    while (fl_readdir(&dirstat, &dirent) == 0) {
      if (!dirent.is_dir) {
        // print file name
        //printf("%s [%d bytes]\n", dirent.filename, dirent.size);
        memcpy(item[n_items++], dirent.filename, SIZE_ITEMS);
      }
    }
    fl_closedir(&dirstat);
  }
  // FL_UNLOCK(&_fs);
  //display_refresh();

  while (1) {

    display_set_cursor(0,0);
    // pulsing header
    display_set_front_back_color((pulse+127)&255,pulse);
    pulse += 7;
    printf("    ===== songs =====    \n\n");
    // list items
    for (int i = 0; i < n_items; ++i) {
      if (i == selected) { // highlight selected
        display_set_front_back_color(0,255);
      } else {
        display_set_front_back_color(255,0);
      }
      printf("%d> %s\n",i,item[i]);
    }
    display_refresh();

    // read buttons and update selection
    if (*BUTTONS & (1<<3)) {
      -- selected;
    }
    if (*BUTTONS & (1<<4)) {
      ++ selected;
    }
    // wrap around
    if (selected < 0) {
      selected = n_items - 1;
    }
    if (selected >= n_items) {
      selected = 0;
    }

  }

}
