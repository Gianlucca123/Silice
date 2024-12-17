// @sylefeb 2022-01-10
// MIT license, see LICENSE_MIT in Silice repo root
// https://github.com/sylefeb/Silice/

#include "config.h"
#include "std.h"
#include "oled.h"
#include "display.h"
#include "printf.h"
#include "sdcard.h"
#include <string.h>
#define MAX_FILENAMES 10
#define MAX_FILENAME_SIZE 32

// include the fat32 library
#include "fat_io_lib/src/fat_filelib.h"

char item[MAX_FILENAMES][MAX_FILENAME_SIZE];
char path_history[50];
char path[MAX_FILENAME_SIZE];
int is_music[MAX_FILENAMES];


void clear_audio()
{
  // wait for a buffer swap (sync)
  int *addr = (int*)(*AUDIO);
  while (addr == (int*)(*AUDIO)) { }
  // go ahead
  for (int b=0 ; b<2 ; ++b) {
    // read directly in hardware buffer
    addr = (int*)(*AUDIO);
    // clear buffer
    memset(addr,0,512);
    // wait for buffer swap
    while (addr == (int*)(*AUDIO)) { }
  }
}

// Function to open and play the selected music file
void openMusic(const char *path_file, const char *file_name) {
  *LEDS = 255;
  memset(display_framebuffer(), 0x00, 128 * 128);
  display_refresh();
  *LEDS = 1;

  display_set_cursor(0, 0);
  display_set_front_back_color(255, 0);
  display_refresh();
  *LEDS = 2;

  // Construct the full file path
  char path[MAX_FILENAME_SIZE];  // Base path
  path[0] = '\0';
  strcat(path, path_file);  // Append the directory path
  strcat(path, "/");  // Add a separator
  const char *end = strcat(path, file_name);  // Append the file name
  if (end - path > 64) {
    *LEDS = 15;
    printf("ERROR path too large\n");
    display_refresh();
    while (1) {}
  }
  *LEDS = 4;

  printf("file: %s\n", path);
  display_refresh();

  // Open the selected music file
  FL_FILE *f = fl_fopen(path, "rb");
  if (f == NULL) {
    // Error: file not found
    printf("not found: %s\n", path);
    display_refresh();
    *LEDS = 8;
    return;
  } else {
    display_set_front_back_color(0, 255);
    printf("file: %s\n", path);
    display_refresh();
    display_set_front_back_color(255, 0);
    printf("playing ... ");
    display_refresh();
    *LEDS = 16;

    int leds = 1;
    int dir = 0;

    // State variable for play/pause
    int is_playing = 1;  // Initially playing

    // Play the entire file
    while (1) {
      int *addr = (int *)(*AUDIO); // Hardware buffer address

      // If music is playing, read directly into the hardware buffer (512 bytes at a time)
      if (is_playing) {
        int sz = fl_fread(addr, 1, 512, f);
        
        // If fewer than 512 bytes were read, it means we reached the end of the file
        if (sz < 512) {
          memset(display_framebuffer(), 0x00, 128 * 128);
          break; // Break when the file is completely read
        }

        // Wait for buffer swap (wait until the buffer is not used by the hardware)
        while (addr == (int *)(*AUDIO)) {
          // You could add a timeout here in case something goes wrong
        }
      }

      // Handle play/pause button (button 6)
      if (*BUTTONS & (1 << 6)) {
        clear_audio();
        while (*BUTTONS & (1 << 6)) {
          // Wait for the button to be released to avoid multiple toggles with a single press
        }

        // Toggle play/pause state
        is_playing = !is_playing;

      
      }

      // Handle stop button (button 5)
      if (*BUTTONS & (1 << 5)) {
        clear_audio();
        break;
      }

      // Light show (optional)
      if (leds == 128 || leds == 1) {
        dir = 1 - dir;
      }
      if (dir) {
        leds = leds << 1;
      } else {
        leds = leds >> 1;
      }
      *LEDS = leds;
    }
    
    // Close the file after playing
    fl_fclose(f);
  }
  *LEDS = 32;
}

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
  memcpy(path, "/", MAX_FILENAME_SIZE);
  // init sdcard
  sdcard_init();
  // initialise File IO Library
  fl_init();
  // attach media access functions to library
  while (fl_attach_media(sdcard_readsector, sdcard_writesector) != FAT_INIT_OK) {
    // keep trying, we need this
  }

  while(1) {
    // header
    memset(display_framebuffer(),0x00,128*128);
    display_refresh();
    display_set_cursor(0,0);
    display_set_front_back_color(0,255);
    //printf("    ===== Musics =====    \n\n");
    display_refresh();
    display_set_front_back_color(255,0);
    // list files (see fl_listdirectory if at_io_lib/src/fat_filelib.c)
    FL_DIR dirstat;
    // FL_LOCK(&_fs);
    if (fl_opendir(path, &dirstat)) {
        struct fs_dir_ent dirent;
        while (fl_readdir(&dirstat, &dirent) == 0) {
                // print file name
                //printf("%s [%d bytes]\n", dirent.filename, dirent.size);
                memcpy(item[n_items], dirent.filename, MAX_FILENAME_SIZE);
                is_music[n_items] = dirent.is_dir ? 0: 1;
                n_items++;
        }
        fl_closedir(&dirstat);
    }
    // FL_UNLOCK(&_fs);
    

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
        if (*BUTTONS & (1<<5)) {
            strcpy(path, path_history);
            n_items = 0;
            break;
        }
        if (*BUTTONS & (1<<6)) {
            if(is_music[selected]) {
                openMusic(path, item[selected]);
            }
            else {
                memcpy(path_history, path, MAX_FILENAME_SIZE);
                strcat(path, item[selected]);
                n_items = 0;
                break;
            }
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

}
