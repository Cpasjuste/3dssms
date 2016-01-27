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
#include <string.h>

#include "3ds/dssms.h"

t_config cfg;

void init() {
    memset(&cfg, 0, sizeof(cfg));
    cfg.fm = 0;
    cfg.japan = TYPE_OVERSEAS;
    cfg.joystick = 0;
    cfg.usesram = 1;
    cfg.fullspeed = 1;
    cfg.nosound = 0;
    cfg.filter = -1;

    osSetSpeedupEnable(true);
    gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
    gfxSetDoubleBuffering(GFX_TOP, false);
    consoleInit(GFX_BOTTOM, NULL);

    if (!cfg.nosound) {
        Result res = csndInit();
        if (res != 0) {
            cfg.nosound = 1;
        } else {
        }
    }

    gspWaitForVBlank();
}

int main(int argc, char **argv) {

    init();
    ds_init(&cfg);
    ds_emulate();
    ds_shutdown();

    if (!cfg.nosound) {
        csndExit();
    }
    gfxExit();

    return 0;
}
