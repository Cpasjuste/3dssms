/*
    Copyright (c) 2002, 2003 Gregory Montoir

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <3ds.h>
#include "shared.h"
#include "3ds/saves.h"
#include "3ds/dssms.h"
#include "picker.h"

#define DS_QUIT 1
#define DS_MENU 2
#define DS_SCREEN_WIDTH 192
#define DS_SCREEN_HEIGHT 240

t_config cfg;

void ds_shutdown();

extern t_config cfg;
static char rom_filename[512];
static u8 *scrBuf;
static u8 *gfxBuf;
static int scrPitch;

static void ds_video_init() {

    scrBuf = linearAlloc(SMS_SCREEN_WIDTH * SMS_SCREEN_HEIGHT * 2);
    scrPitch = 240 * 2;
}

static void ds_video_finish_update() {

    int mx, my, mxdest, mydest;
    int bx, by, i, j;

    int w = SMS_SCREEN_HEIGHT;
    int h = SMS_SCREEN_WIDTH;
    mxdest = w / 2;
    mydest = h / 2;
    mx = bitmap.width / 2;
    my = bitmap.height / 2;
    int x_ofs = (400 - SMS_SCREEN_WIDTH) / 2;
    int y_ofs = (240 - SMS_SCREEN_HEIGHT) / 2;

    gfxBuf = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            bx = mx + (j - mydest);
            by = my + (i - mxdest) * -1;

            u32 pixel = *(u16 *) (bitmap.data + by * bitmap.pitch + bx * 2);
            if (bx >= 0 && bx < h && by >= 0 && by < w)
                *(u16 *) (gfxBuf + (j + x_ofs) * scrPitch + (i + y_ofs) * 2) = pixel;
        }
    }

    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

static void ds_video_update() {
    sms_frame(0);
    ds_video_finish_update();
}

static void ds_video_close() {
    linearFree(scrBuf);
}

void ds_audio_update() {

    csndPlaySound(0x8,
                  SOUND_ONE_SHOT | SOUND_FORMAT_16BIT, SOUND_FREQUENCY,
                  1.0f, 0.0f, (u32 *) snd.buffer[0], (u32 *) snd.buffer[1], snd.bufsize);
}

void ds_sound_init() {
    CSND_SetPlayState(0x8, 1);
    csndExecCmds(0);
}

int ds_init(const t_config *pcfg) {

    pick_file(rom_filename, "/3ds/sms/roms");
    if (load_rom(rom_filename) == 0) {
        printf("ERROR: can't load `%s'.\n", rom_filename);
        return 0;
    }

    ds_video_init();

    sms.use_fm = pcfg->fm;
    sms.country = pcfg->japan ? TYPE_DOMESTIC : TYPE_OVERSEAS;
    sms.save = pcfg->usesram;
    memset(&bitmap, 0, sizeof(t_bitmap));
    bitmap.width = SMS_SCREEN_WIDTH;   // 256
    bitmap.height = SMS_SCREEN_HEIGHT;  // 192
    bitmap.depth = 16;
    bitmap.pitch = SMS_SCREEN_WIDTH * 2;
    bitmap.data = scrBuf;
    system_init(pcfg->nosound ? 0 : SOUND_FREQUENCY);
    load_sram(pcfg->game_name);

    if (!cfg.nosound) {
        ds_sound_init();
    }

    printf("\n\n");
    printf("     Press L+R to change game...\n");
    printf("     Press SELECT to quit...\n");

    return 1;
}

int ds_pad_update() {

    hidScanInput();
    u32 k = hidKeysHeld();

    if (k & KEY_SELECT)
        return DS_QUIT;

    if (k & KEY_START) {
        input.system |= (IS_GG) ? INPUT_START : INPUT_PAUSE;
    } else
        input.system &= (IS_GG) ? ~INPUT_START : ~INPUT_PAUSE;

    if (k & KEY_UP) {
        input.pad[0] |= INPUT_UP;
    } else
        input.pad[0] &= ~INPUT_UP;

    if (k & KEY_DOWN) {
        input.pad[0] |= INPUT_DOWN;
    } else
        input.pad[0] &= ~INPUT_DOWN;

    if (k & KEY_LEFT) {
        input.pad[0] |= INPUT_LEFT;
    } else
        input.pad[0] &= ~INPUT_LEFT;

    if (k & KEY_RIGHT) {
        input.pad[0] |= INPUT_RIGHT;
    } else
        input.pad[0] &= ~INPUT_RIGHT;

    if (k & KEY_A) {
        input.pad[0] |= INPUT_BUTTON1;
    } else
        input.pad[0] &= ~INPUT_BUTTON1;

    if (k & KEY_B) {
        input.pad[0] |= INPUT_BUTTON2;
    } else
        input.pad[0] &= ~INPUT_BUTTON2;

    if (k & KEY_L && k & KEY_R) {
        return DS_MENU;
        //input.system |= INPUT_HARD_RESET;
    } //else input.system &= ~INPUT_HARD_RESET;

    return 0;
}

int ds_input_update() {

    int event = ds_pad_update();
    if (event == DS_QUIT) {
        return 0;
    }

    if (event == DS_MENU) {
        ds_shutdown();
        ds_init(&cfg);
    }
    return 1;
}

void ds_emulate() {

    while (ds_input_update()) {

        ds_input_update();
        ds_video_update();
        if (!cfg.nosound) {
            ds_audio_update();
        }
    }
}

void ds_shutdown() {
    if (!cfg.nosound) {
        CSND_SetPlayState(0x8, 0);
        csndExecCmds(0);
    }
    save_sram(rom_filename);
    system_reset();
    system_shutdown();
    ds_video_close();
}
