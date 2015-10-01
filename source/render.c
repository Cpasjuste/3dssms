
#include "shared.h"

/* Background drawing function */
void (*render_bg)(int line);

/* Pointer to output buffer */
uint8 *linebuf;

/* Internal buffer for drawing non 8-bit displays */
uint8 internal_buffer[0x100];

/* Precalculated pixel table */
uint16 pixel[PALETTE_SIZE];

/* Pattern cache */
uint8 cache[0x20000];

/* Dirty pattern info */
uint8 vram_dirty[0x200];
uint8 is_vram_dirty;

/* Pixel look-up table */
uint8 lut[0x10000];

/* Attribute expansion table */
uint32 atex[4] =
{
    0x00000000,
    0x10101010,
    0x20202020,
    0x30303030,
};

/* Display sizes */
int vp_vstart;
int vp_vend;
int vp_hstart;
int vp_hend;


/* Macros to access memory 32-bits at a time (from MAME's drawgfx.c) */

#ifdef ALIGN_DWORD

static __inline__ uint32 read_dword(void *address)
{
    if ((uint32)address & 3)
	{
#ifdef LSB_FIRST  /* little endian version */
        return ( *((uint8 *)address) +
                (*((uint8 *)address+1) << 8)  +
                (*((uint8 *)address+2) << 16) +
                (*((uint8 *)address+3) << 24) );
#else             /* big endian version */
        return ( *((uint8 *)address+3) +
                (*((uint8 *)address+2) << 8)  +
                (*((uint8 *)address+1) << 16) +
                (*((uint8 *)address)   << 24) );
#endif
	}
	else
        return *(uint32 *)address;
}


static __inline__ void write_dword(void *address, uint32 data)
{
    if ((uint32)address & 3)
	{
#ifdef LSB_FIRST
            *((uint8 *)address) =    data;
            *((uint8 *)address+1) = (data >> 8);
            *((uint8 *)address+2) = (data >> 16);
            *((uint8 *)address+3) = (data >> 24);
#else
            *((uint8 *)address+3) =  data;
            *((uint8 *)address+2) = (data >> 8);
            *((uint8 *)address+1) = (data >> 16);
            *((uint8 *)address)   = (data >> 24);
#endif
		return;
  	}
  	else
        *(uint32 *)address = data;
}
#else
#define read_dword(address) *(uint32 *)address
#define write_dword(address,data) *(uint32 *)address=data
#endif


/****************************************************************************/


/* Initialize the rendering data */
void render_init(void)
{
    int bx, sx, b, s, bp, bf, sf, c;

    /* Generate 64k of data for the look up table */
    for(bx = 0; bx < 0x100; bx += 1)
    {
        for(sx = 0; sx < 0x100; sx += 1)
        {
            /* Background pixel */
            b  = (bx & 0x0F);

            /* Background priority */
            bp = (bx & 0x20) ? 1 : 0;

            /* Full background pixel + priority + sprite marker */
            bf = (bx & 0x7F);

            /* Sprite pixel */
            s  = (sx & 0x0F);

            /* Full sprite pixel, w/ palette and marker bits added */
            sf = (sx & 0x0F) | 0x10 | 0x40;

            /* Overwriting a sprite pixel ? */
            if(bx & 0x40)
            {
                /* Return the input */
                c = bf;
            }
            else
            {
                /* Work out priority and transparency for both pixels */
                c = bp ? b ? bf : s ? sf : bf : s ? sf : bf;
            }

            /* Store result */
            lut[(bx << 8) | (sx)] = c;
        }
    }

    render_reset();
}


/* Reset the rendering data */
void render_reset(void)
{
    int i;

    /* Clear display bitmap */
    memset(bitmap.data, 0, bitmap.pitch * bitmap.height);

    /* Clear palette */
    for(i = 0; i < PALETTE_SIZE; i += 1)
    {
        palette_sync(i);
    }

    /* Invalidate pattern cache */
    is_vram_dirty = 1;
    memset(vram_dirty, 1, 0x200);
    memset(cache, 0, sizeof(cache));

    /* Set up viewport size */
    if(IS_GG)
    {
        vp_vstart = 24;
        vp_vend   = 168;
        vp_hstart = 6;
        vp_hend   = 26;
    }
    else
    {
        vp_vstart = 0;
        vp_vend   = 192;
        vp_hstart = 0;
        vp_hend   = 32;
    }

    /* Pick render routine */
    render_bg = IS_GG ? render_bg_gg : render_bg_sms;
}


/* Draw a line of the display */
void render_line(int line)
{
    /* Ensure we're within the viewport range */
    if((line < vp_vstart) || (line >= vp_vend)) return;

    /* Point to current line in output buffer */
    linebuf = (bitmap.depth == 8) ? &bitmap.data[(line * bitmap.pitch)] : &internal_buffer[0];

    /* Update pattern cache */
    update_cache();

    /* Blank line */
    if( (!(vdp.reg[1] & 0x40)) || (((vdp.reg[2] & 1) == 0) && (IS_SMS)))
    {
        memset(linebuf + (vp_hstart << 3), BACKDROP_COLOR, BMP_WIDTH);
    }
    else
    {
        /* Draw background */
        render_bg(line);

        /* Draw sprites */
        render_obj(line);

        /* Blank leftmost column of display */
        if(vdp.reg[0] & 0x20)
        {
            memset(linebuf, BACKDROP_COLOR, 8);
        }
    }

    if(bitmap.depth != 8) remap_8_to_16(line);
}


/* Draw the Master System background */
void render_bg_sms(int line)
{
    int locked = 0;
    int v_line = (line + vdp.reg[9]) % 224;
    int v_row  = (v_line & 7) << 3;
    int hscroll = ((vdp.reg[0] & 0x40) && (line < 0x10)) ? 0 : (0x100 - vdp.reg[8]);
    int column = vp_hstart;
    uint16 attr;
    uint16 *nt = (uint16 *)&vdp.vram[vdp.ntab + ((v_line >> 3) << 6)];
    int nt_scroll = (hscroll >> 3);
    int shift = (hscroll & 7);
    uint32 atex_mask;
    uint32 *cache_ptr;
    uint32 *linebuf_ptr = (uint32 *)&linebuf[0 - shift];

    /* Draw first column (clipped) */
    if(shift)
    {
        int x, c, a;

        attr = nt[(column + nt_scroll) & 0x1F];

#ifndef LSB_FIRST
        attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
        a = (attr >> 7) & 0x30;

        for(x = shift; x < 8; x += 1)
        {
            c = cache[((attr & 0x7FF) << 6) | (v_row) | (x)];
            linebuf[(0 - shift) + (x)  ] = ((c) | (a));
        }

        column += 1;
    }

    /* Draw a line of the background */
    for(; column < vp_hend; column += 1)
    {
        /* Stop vertical scrolling for leftmost eight columns */
        if((vdp.reg[0] & 0x80) && (!locked) && (column >= 24))
        {
            locked = 1;
            v_row = (line & 7) << 3;
            nt = (uint16 *)&vdp.vram[((vdp.reg[2] << 10) & 0x3800) + ((line >> 3) << 6)];
        }

        /* Get name table attribute word */
        attr = nt[(column + nt_scroll) & 0x1F];

#ifndef LSB_FIRST
        attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
        /* Expand priority and palette bits */
        atex_mask = atex[(attr >> 11) & 3];

        /* Point to a line of pattern data in cache */
        cache_ptr = (uint32 *)&cache[((attr & 0x7FF) << 6) | (v_row)];
        
        /* Copy the left half, adding the attribute bits in */
        write_dword( &linebuf_ptr[(column << 1)] , read_dword( &cache_ptr[0] ) | (atex_mask));

        /* Copy the right half, adding the attribute bits in */
        write_dword( &linebuf_ptr[(column << 1) | (1)], read_dword( &cache_ptr[1] ) | (atex_mask));
    }

    /* Draw last column (clipped) */
    if(shift)
    {
        int x, c, a;

        char *p = (char *)&linebuf[(0 - shift)+(column << 3)];

        attr = nt[(column + nt_scroll) & 0x1F];

#ifndef LSB_FIRST
        attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
        a = (attr >> 7) & 0x30;

        for(x = 0; x < shift; x += 1)
        {
            c = cache[((attr & 0x7FF) << 6) | (v_row) | (x)];
            p[x] = ((c) | (a));
        }
    }
}


/* Draw the Game Gear background */
void render_bg_gg(int line)
{
    int v_line = (line + vdp.reg[9]) % 224;
    int v_row  = (v_line & 7) << 3;
    int hscroll = (0x100 - vdp.reg[8]);
    int column;
    uint16 attr;
    uint16 *nt = (uint16 *)&vdp.vram[vdp.ntab + ((v_line >> 3) << 6)];
    int nt_scroll = (hscroll >> 3);
    uint32 atex_mask;
    uint32 *cache_ptr;
    uint32 *linebuf_ptr = (uint32 *)&linebuf[0 - (hscroll & 7)];

    /* Draw a line of the background */
    for(column = vp_hstart; column <= vp_hend; column += 1)
    {
        /* Get name table attribute word */
        attr = nt[(column + nt_scroll) & 0x1F];

#ifndef LSB_FIRST
        attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
        /* Expand priority and palette bits */
        atex_mask = atex[(attr >> 11) & 3];

        /* Point to a line of pattern data in cache */
        cache_ptr = (uint32 *)&cache[((attr & 0x7FF) << 6) | (v_row)];

        /* Copy the left half, adding the attribute bits in */
        write_dword( &linebuf_ptr[(column << 1)] , read_dword( &cache_ptr[0] ) | (atex_mask));

        /* Copy the right half, adding the attribute bits in */
        write_dword( &linebuf_ptr[(column << 1) | (1)], read_dword( &cache_ptr[1] ) | (atex_mask));
    }
}


/* Draw sprites */
void render_obj(int line)
{
    int i;

    /* Sprite count for current line (8 max.) */
    int count = 0;

    /* Sprite dimensions */
    int width = 8;
    int height = (vdp.reg[1] & 0x02) ? 16 : 8;

    /* Pointer to sprite attribute table */
    uint8 *st = (uint8 *)&vdp.vram[vdp.satb];

    /* Adjust dimensions for double size sprites */
    if(vdp.reg[1] & 0x01)
    {
        width *= 2;
        height *= 2;
    }

    /* Draw sprites in front-to-back order */
    for(i = 0; i < 64; i += 1)
    {
        /* Sprite Y position */
        int yp = st[i];

        /* End of sprite list marker? */
        if(yp == 208) return;

        /* Actual Y position is +1 */
        yp += 1;

        /* Wrap Y coordinate for sprites > 240 */
        if(yp > 240) yp -= 256;

        /* Check if sprite falls on current line */
        if((line >= yp) && (line < (yp + height)))
        {
            uint8 *linebuf_ptr;

            /* Width of sprite */
            int start = 0;
            int end = width;

            /* Sprite X position */
            int xp = st[0x80 + (i << 1)];

            /* Pattern name */
            int n = st[0x81 + (i << 1)];

            /* Bump sprite count */
            count += 1;

            /* Too many sprites on this line ? */
            if((vdp.limit) && (count == 9)) return;

            /* X position shift */
            if(vdp.reg[0] & 0x08) xp -= 8;

            /* Add MSB of pattern name */
            if(vdp.reg[6] & 0x04) n |= 0x0100;

            /* Mask LSB for 8x16 sprites */
            if(vdp.reg[1] & 0x02) n &= 0x01FE;

            /* Point to offset in line buffer */
            linebuf_ptr = (uint8 *)&linebuf[xp];

            /* Clip sprites on left edge */
            if(xp < 0)
            {
                start = (0 - xp);
            }

            /* Clip sprites on right edge */
            if((xp + width) > 256)        
            {
                end = (256 - xp);
            }

            /* Draw double size sprite */
            if(vdp.reg[1] & 0x01)
            {
                int x;
                uint8 *cache_ptr = (uint8 *)&cache[(n << 6) | (((line - yp) >> 1) << 3)];

                /* Draw sprite line */
                for(x = start; x < end; x += 1)
                {
                    /* Source pixel from cache */
                    uint8 sp = cache_ptr[(x >> 1)];
    
                    /* Only draw opaque sprite pixels */
                    if(sp)
                    {
                        /* Background pixel from line buffer */
                        uint8 bg = linebuf_ptr[x];
    
                        /* Look up result */
                        linebuf_ptr[x] = lut[(bg << 8) | (sp)];
    
                        /* Set sprite collision flag */
                        if(bg & 0x40) vdp.status |= 0x20;
                    }
                }
            }
            else /* Regular size sprite (8x8 / 8x16) */
            {
                int x;
                uint8 *cache_ptr = (uint8 *)&cache[(n << 6) | ((line - yp) << 3)];

                /* Draw sprite line */
                for(x = start; x < end; x += 1)
                {
                    /* Source pixel from cache */
                    uint8 sp = cache_ptr[x];
    
                    /* Only draw opaque sprite pixels */
                    if(sp)
                    {
                        /* Background pixel from line buffer */
                        uint8 bg = linebuf_ptr[x];
    
                        /* Look up result */
                        linebuf_ptr[x] = lut[(bg << 8) | (sp)];
    
                        /* Set sprite collision flag */
                        if(bg & 0x40) vdp.status |= 0x20;
                    }
                }
            }
        }
    }
}


/* Update pattern cache with modified tiles */
void update_cache(void)
{
    int i, x, y, c;
    int b0, b1, b2, b3;
    int i0, i1, i2, i3;

    if(!is_vram_dirty) return;
    is_vram_dirty = 0;

    for(i = 0; i < 0x200; i += 1)
    {
        if(vram_dirty[i])
        {
            vram_dirty[i] = 0;

            for(y = 0; y < 8; y += 1)
            {
                b0 = vdp.vram[(i << 5) | (y << 2) | (0)];
                b1 = vdp.vram[(i << 5) | (y << 2) | (1)];
                b2 = vdp.vram[(i << 5) | (y << 2) | (2)];
                b3 = vdp.vram[(i << 5) | (y << 2) | (3)];

                for(x = 0; x < 8; x += 1)
                {
                    i0 = (b0 >> (x ^ 7)) & 1;
                    i1 = (b1 >> (x ^ 7)) & 1;
                    i2 = (b2 >> (x ^ 7)) & 1;
                    i3 = (b3 >> (x ^ 7)) & 1;

                    c = (i3 << 3 | i2 << 2 | i1 << 1 | i0);

                    cache[0x00000 | (i << 6) | ((y  ) << 3) | (x)] = c;
                    cache[0x08000 | (i << 6) | ((y  ) << 3) | (x ^ 7)] = c;
                    cache[0x10000 | (i << 6) | ((y ^ 7) << 3) | (x)] = c;
                    cache[0x18000 | (i << 6) | ((y ^ 7) << 3) | (x ^ 7)] = c;
                }
            }
        }
    }
}


/* Update a palette entry */
void palette_sync(int index)
{
    int r, g, b;

    if(IS_GG)
    {
        r = ((vdp.cram[(index << 1) | 0] >> 1) & 7) << 5;
        g = ((vdp.cram[(index << 1) | 0] >> 5) & 7) << 5;
        b = ((vdp.cram[(index << 1) | 1] >> 1) & 7) << 5;
    }
    else
    {
        r = ((vdp.cram[index] >> 0) & 3) << 6;
        g = ((vdp.cram[index] >> 2) & 3) << 6;
        b = ((vdp.cram[index] >> 4) & 3) << 6;
    }

    bitmap.pal.color[index][0] = r;
    bitmap.pal.color[index][1] = g;
    bitmap.pal.color[index][2] = b;

    pixel[index] = MAKE_PIXEL(r, g, b);

    bitmap.pal.dirty[index] = bitmap.pal.update = 1;
}


void remap_8_to_16(int line)
{
    int i;
    int length = BMP_WIDTH;
    int ofs = BMP_X_OFFSET;
    uint16 *p = (uint16 *)&bitmap.data[(line * bitmap.pitch) + (ofs << 1)];

    for(i = 0; i < length; i += 1)
    {
        p[i] = pixel[(internal_buffer[(ofs + i)] & PIXEL_MASK)];
    }        
}







