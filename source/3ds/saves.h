#ifndef __SAVES_H__
#define __SAVES_H__

#ifdef __cplusplus
extern "C" {
#endif

extern int load_rom(const char *);

extern void load_sram(const char *);

extern void save_sram(const char *);

extern int save_state(const char *, int);

extern int load_state(const char *, int);

#ifdef __cplusplus
}
#endif

#endif
