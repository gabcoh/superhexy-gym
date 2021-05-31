#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ulimit.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>

#include <GL/gl.h>
#include <SDL.h>
#include <SDL_pixels.h>

#ifdef DEBUG
#define LOG(...) if (enabled) {printf(__VA_ARGS__);}
#else
#define LOG(...)
#endif

static int enabled = 1;
static int sock_fd = -1;
static int inited = 0;
static void *shared_mem = NULL;
static int press_left = 0;
static int press_right = 0;
static int press_enter = 0;
static uint64_t millis = 0;

typedef void ofAppGlutWindow_t(void);
ofAppGlutWindow_t* ofAppGlutWindow = (ofAppGlutWindow_t*)0x004d9f80;
typedef void mixAllPlayers_t(void);
mixAllPlayers_t* mixAllPlayers = (mixAllPlayers_t*)0x004f00b0;
typedef void genericKeyPoll_t(void*);
genericKeyPoll_t* genericKeyPoll = (genericKeyPoll_t*)0x458710;
typedef uint64_t ofGetElapsedTimeMillis_t(void);
ofGetElapsedTimeMillis_t* ofGetElapsedTimeMillis = (ofGetElapsedTimeMillis_t*)0x410ca0;
typedef void ofSetWindowShape_t(int, int);
ofSetWindowShape_t* ofSetWindowShape = (ofSetWindowShape_t*)0x004dba70;

static void **sdl_window = (void**)0x00baf4a8;
static void *ofAppPtr = (void*)0xbaf4c8;
#define OF_APP_PTR_OFF(x) (*(int*)(*(void**)ofAppPtr + x))
#define KP_RIGHT OF_APP_PTR_OFF(0x40)
#define KP_LEFT OF_APP_PTR_OFF(0x3f)
#define KP_ENTER OF_APP_PTR_OFF(0x41)
#define SCREEN_INDICATOR OF_APP_PTR_OFF(0x53d4)
#define LEVEL OF_APP_PTR_OFF(0x5414)
#define TIME OF_APP_PTR_OFF(0x28d0)
#define DEATH_COUNTER OF_APP_PTR_OFF(0xfc)
static int *width = (int*)0xbaf4b8;
static int *height = (int*)0xbaf4bc;
static int *REQUESTED_HEIGHT = (int*)0xbaf464;
static int *REQUESTED_WIDTH = (int*)0xbaf460;


struct init_message {
  int width;
  int height;
  int depth;
  int actions;
};
struct init_shm_message {
  int size;
  char name[64];
};
struct train_message {
  int terminal;
  float reward;
};

__attribute__((destructor)) void deinit(void) {
  if (!inited) {
    return;
  }
  LOG("TODO: destructor\n");
  shutdown(SHUT_RDWR, sock_fd);
  close(sock_fd);
}

void init(void) {
  if (inited) {
    return;
  }
  if (getenv("SHIM_DISABLED")) {
    enabled = 0;
    inited = 1;
    return;
  }
  // Nearly gave myself a heart attack
  // Don't really need this anymore, but I guess it couldn't hurt
  if (!ulimit(UL_SETFSIZE, 1024*1024)) {
    perror("ulimit");
    exit(1);
  }
  
  sock_fd = socket(AF_UNIX, SOCK_SEQPACKET|SOCK_CLOEXEC, 0);
  if (!sock_fd) {
    perror("socket");
    exit(1);
  }
  char *sock_path = getenv("SHIM_SOCKET");
  if (!sock_path) {
    fprintf(stderr, "SHIM_SOCKET env var not defined\n");
    exit(1);
  }
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, sock_path);

  if (-1 == connect(sock_fd, (const struct sockaddr*)&addr, sizeof(addr))) {
    perror("connect");
    exit(1);
  }
  struct init_message init_mess;
  init_mess.width = *REQUESTED_WIDTH;
  init_mess.height = *REQUESTED_HEIGHT;
  init_mess.depth = 3;
  init_mess.actions = 3;
  if (sizeof(struct init_message) != write(sock_fd, &init_mess, sizeof(struct init_message))) {
    // unix sock should not have partial write (I think)
    perror("write");
    exit(1);
  }

  struct init_shm_message shm_mess;
  read(sock_fd, &shm_mess, sizeof(struct init_shm_message));
  
  int shm_fd = shm_open(shm_mess.name, O_RDWR, 0);

  shared_mem = mmap(0, shm_mess.size, PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if (MAP_FAILED == shared_mem) {
    perror("mmap");
    exit(1);
  }

  inited = 1;
}

void copy_pixels(void) {
  /* int w, h; */
  /* SDL_GL_GetDrawableSize(*sdl_window, &w, &h); */
  /* printf("Drawable size: %d %d\n", w, h); */
  /* SDL_Surface *surf = SDL_GetWindowSurface(*sdl_window); */
  /* SDL_LockSurface(surf); */
  /* memcpy(shared_mem, surf->pixels, (*width)*(*height)*3); */

  /* SDL_UnlockSurface(surf); */

  glReadBuffer(GL_FRONT);
  glReadPixels(0, 0, *width, *height, GL_RGB, GL_UNSIGNED_BYTE, shared_mem);

}
void custom_key_poll() {
  KP_RIGHT = 0;
  KP_LEFT = 0;
  KP_ENTER = 0;
  if (press_left) {
    OF_APP_PTR_OFF(0x40) = 1;
    KP_LEFT = 1;
  }
  if (press_right) {
    KP_RIGHT = 1;
  }
  if (press_enter) {
    KP_ENTER = 1;
  }
  press_left = 0;
  press_right = 0;
  press_enter = 0;
}
void start_game(int level) {
  if (SCREEN_INDICATOR == -1) {
    // if in entry screen hit enter to go to levels
    press_enter = 1;
  } else if (SCREEN_INDICATOR == -2 || SCREEN_INDICATOR >= 0) {
    if (LEVEL != level) {
      press_left = 1;
    } else {
      press_enter = 1;
    }
  } else {
    LOG("Tried to start game while in game or in unknown state");
  }
}
int in_game() {
  return (SCREEN_INDICATOR >= 0) && (DEATH_COUNTER == 0);
}
void pre_draw(void) {
  LOG ("Pre-draw\n");
  millis += 80;

  // todo this is where I was
  return;
}

void post_draw(void) {
  static int called_count = 0;
  static int joining_game = 0;
  static int previous_time = 0;
  copy_pixels();

  LOG("Done Drawing\n");
  LOG("Called %d times\n", called_count);
  called_count++;


  if (in_game() && (joining_game == 0 || joining_game > 10)) {
    joining_game = 0;
    struct train_message mess;
    mess.terminal = 0;
    int temp = previous_time;
    previous_time = TIME;
    //mess.reward = (float)(TIME - temp)/60.0;
    mess.reward = 1;
    if (sizeof(struct train_message) != write(sock_fd, &mess, sizeof(struct train_message))) {
      perror("write");
      exit(1);
    }
    int action;
    if (sizeof(int) != read(sock_fd, &action, sizeof(int))) {
      perror("read");
      exit(1);
    }
    switch (action) {
    case 0:
      press_left = 1;
      break;
    case 1:
      press_right = 1;
      break;
    case 2:
      break;
    default:
      fprintf(stderr, "Recieved invalid action\n");
      exit(1);
    }
  } else if (in_game()) {
    joining_game++;
  } else if (joining_game == 0) {
    struct train_message mess;
    mess.terminal = 1;
    mess.reward = -1;
    if (sizeof(struct train_message) != write(sock_fd, &mess, sizeof(struct train_message))) {
      perror("write");
      exit(1);
    }
    // Actoin doesn'tmatter, just a chance to wait
    int action;
    if (sizeof(int) != read(sock_fd, &action, sizeof(int))) {
      perror("read");
      exit(1);
    }
    joining_game = 1;
    previous_time = 0;
    start_game(0);
  } else {
    start_game(0);
  }
}

uint64_t __stack_chk_fail(uint64_t arg1) {
  register void* rbp asm ("rbp");
  void *ret = *((void**)(rbp + 0x8));
  LOG ("Return address: %p\n", ret);

  printf("requested: %d %d\n", *REQUESTED_WIDTH, *REQUESTED_HEIGHT);
  printf("%d %d\n", *width, *height);
  //*width = 1024;
  //*height = 768;
  *REQUESTED_HEIGHT = 480;
  *REQUESTED_WIDTH = 768;
  ofSetWindowShape(*REQUESTED_WIDTH, *REQUESTED_HEIGHT);

  init();
  
  switch ((long)ret) {
  case 0x4da36a:
    if (enabled) {
      pre_draw();
    }
    ofAppGlutWindow();
    return 0;
  case 0x4da36f:
    if (enabled) {
      post_draw();
    }
    mixAllPlayers();
    return 0;
  case 0x4593c6:
    if (enabled) {
      custom_key_poll();
    } else {
      genericKeyPoll(*(void**)ofAppPtr);
    }
    return 0;
  case 0x004da2a6:
    // usleep
    if (!enabled) {
      return usleep(arg1);
    }
    return 0;
  case 0x4910f0:
  case 0x4da179:
    // Can I do this here?
    if (!enabled) {
      return ofGetElapsedTimeMillis();
    }
    return millis;
  default:
    LOG("__stack_chk_fail called from unknown place\n");
    return 0;
  }
}
