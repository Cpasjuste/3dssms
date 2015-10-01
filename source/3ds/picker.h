#ifndef _picker_h_
#define _picker_h_

#ifdef __cplusplus
extern "C" {
#endif

void debug(const char *msg, ...);
void pick_file(char *picked, const char *path);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // _picker_h_