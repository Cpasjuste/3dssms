
#ifndef __SDLSMS_H__
#define __SDLSMS_H__

#include "shared.h"

#define SMSSDL_CORE_VER  VERSION
#define SMSSDL_RELEASE   "7"
#define SMSSDL_TITLE     "SMS Plus/SDL v" SMSSDL_CORE_VER "R" SMSSDL_RELEASE

#define SMS_SCREEN_WIDTH  256
#define SMS_SCREEN_HEIGHT 192
#define GG_SCREEN_WIDTH   160
#define GG_SCREEN_HEIGHT  144

#define FILTER_MARGIN_HEIGHT 3

#define SOUND_FREQUENCY    22050
#ifndef __QNXNTO__
#define SOUND_SAMPLES_SIZE 2048
#else
#define SOUND_SAMPLES_SIZE 512
#endif

#define MACHINE_FPS 60

typedef struct {
  int up,down,left,right;
  int b1,b2;
  int start;
} t_paddle;

typedef struct {
  int fm;
  int japan;
  int usesram;
  int frameskip;
  int fullscreen;
  int filter;
  int fullspeed;
  int nosound;
  int joystick;
  char game_name[0x100];
} t_config;

extern int ds_init(const t_config*);
extern void ds_emulate();
extern void ds_shutdown();



#endif
