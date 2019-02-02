#ifndef __WII_MOUSE_H__
#define __WII_MOUSE_H__ 1

int mouse_init(const char *device);
void mouse_send_lmb(int fd, int value);
void mouse_send_wheel(int fd, int value);
void mouse_move_relative(int fd, int x, int y);
void mouse_close(int fd);

#endif /* __WII_MOUSE_H__ */
