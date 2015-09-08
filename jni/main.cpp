#include <cstdlib>
#include <unistd.h>
#include <cassert>
#include <stdint.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <cstdio>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#include <linux/fb.h>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <iostream>

// https://android.googlesource.com/platform/system/core.git/+/android-4.0.4_r2.1/toolbox/sendevent.c
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};

#define EVIOCGVERSION		_IOR('E', 0x01, int)			/* get driver version */

class EventDevice {
public:
  EventDevice() : fd_(OpenDevice()), event_id_(0) {}
  ~EventDevice() {
    if (fd_ > 0)
      close(fd_);
  }
  EventDevice(const EventDevice&) = delete;
  EventDevice& operator=(const EventDevice&) = delete;

  bool SendEvent(__u16 type, __u16 code, __s32 value) const;
  bool Touch(int x, int y);

private:
  const int fd_;
  int event_id_;
  static int OpenDevice();
};


int EventDevice::OpenDevice() {
  const static char dev[] = "/dev/input/event2";
  int fd = open(dev, O_RDWR);
  if(fd < 0) {
    fprintf(stderr, "could not open %s, %s\n", dev, strerror(errno));
    return 0;
  }
  int version;
  if (ioctl(fd, EVIOCGVERSION, &version)) {
    fprintf(stderr, "could not get driver version for %s, %s\n", dev, strerror(errno));
    return 0;
  }
  return fd;
}

bool EventDevice::SendEvent(__u16 type, __u16 code, __s32 value) const {
  if (fd_ == 0) {
    return false;
  }
  struct input_event event;
  memset(&event, 0, sizeof(event));
  event.type = type;
  event.code = code;
  event.value = value;
  int ret = write(fd_, &event, sizeof(event));
  if (ret < (int)sizeof(event)) {
    fprintf(stderr, "write event failed, %s\n", strerror(errno));
    return false;
  }
  return true;
}

bool EventDevice::Touch(int x, int y) {
  std::cout << "t (" << x << ", " << y << ")" << std::endl;

  return SendEvent(3, 57, event_id_++)
    && SendEvent(3, 58, 29)
    && SendEvent(3, 53, x)
    && SendEvent(3, 54, y)
    && SendEvent(0, 0, 0)
    && SendEvent(3, 57, 0xffffffff)
    && SendEvent(0, 0, 0);
}

// https://gist.github.com/amitn/9658326
class Screen {
public:
  Screen();
  ~Screen() { if (fd_ > 0) close(fd_); }
  cv::Mat Capture() {
    cv::Mat cpy;
    cv::cvtColor(buf_, cpy, CV_RGBA2BGR);
    return cpy;
  }
  Screen(const Screen&) = delete;
  Screen& operator=(const Screen&) = delete;

private:
  cv::Size resolution_;
  int fd_;
  cv::Mat buf_;
};

Screen::Screen() {
  struct fb_var_screeninfo vscreeninfo;
  struct fb_fix_screeninfo fscreeninfo;
  fd_ = open("/dev/graphics/fb0", O_RDWR);
  if (fd_ < 0)
    return;

  if (ioctl(fd_, FBIOGET_VSCREENINFO, &vscreeninfo) < 0) {
    return;
  }

  if (ioctl(fd_, FBIOGET_FSCREENINFO, &fscreeninfo) < 0) {
    return;
  }

  void* data = mmap(0, fscreeninfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  buf_ = cv::Mat(cv::Size(vscreeninfo.xres, vscreeninfo.yres), CV_8UC4, data, fscreeninfo.line_length);
}

const double kThreshold = 0.03;
const cv::Point kVoidPoint(-1, -1);
const int kSleepMs = 200;

struct Command {
  std::string temp;
  cv::Mat img;
  cv::Point left_top;
  double threshold;
  cv::Point repeat_point;
  int sleep_ms;

  Command(std::string const& t, cv::Point const& lt, double h, cv::Point const& p, int s) :
    temp(t),
    img(cv::imread("templates/" + t + ".png")),
    left_top(lt),
    threshold(h),
    repeat_point(p),
    sleep_ms(s) {}
  Command(std::string const& t, cv::Point const& lt, double h, cv::Point const& p) : Command(t, lt, h, p, kSleepMs) {}
  Command(std::string const& t, cv::Point const& lt, double h) : Command(t, lt, h, kVoidPoint) {}
  Command(std::string const& t, cv::Point const& lt) : Command(t, lt, kThreshold) {}

  bool hit(cv::Mat const& cap);
};

bool Command::hit(cv::Mat const& cap) {
  cv::Mat roi = cap(cv::Rect(left_top, img.size()));
  cv::Mat result;
  cv::matchTemplate(roi, img, result, CV_TM_SQDIFF_NORMED);
  assert(result.total() == 1);
  return result.at<float>(0, 0) < threshold;
}

int run(Screen& screen, EventDevice& ev, int init_point = 0) {
  Command commands[] = {
    {"001", cv::Point(366, 428), kThreshold, cv::Point(360, 640)},
    {"002", cv::Point(56, 309)},
    {"003", cv::Point(161, 451)},
    {"004", cv::Point(148, 673)},
    {"100", cv::Point(648, 1128)},
    {"101", cv::Point(179, 665)},
    {"102", cv::Point(74, 845), kThreshold, kVoidPoint, 500},
    {"102", cv::Point(225, 703)  , kThreshold, kVoidPoint, 500},
    {"100", cv::Point(648, 1128) },
    {"101", cv::Point(169, 665)  },
    {"104", cv::Point(60, 951)   },
    {"105", cv::Point(41, 565)   },
    {"107", cv::Point(209, 611), kThreshold, cv::Point(662, 1210)}, // スキップをスカる対策
    {"100", cv::Point(648, 1128) },
    {"101", cv::Point(179, 665)  },
    {"108", cv::Point(659, 1113), 0.02},
    {"109", cv::Point(211, 708), 0.03},
    {"115", cv::Point(160, 608) , kThreshold, cv::Point(69, 1029)},
    {"116", cv::Point(50, 961)  },
    {"117", cv::Point(213, 495) },
    {"100", cv::Point(648, 1128)},
    {"101", cv::Point(179, 665) },
    {"118", cv::Point(52, 975)  },
    {"119", cv::Point(503, 601) },
    {"120", cv::Point(83, 706)  },
    {"110", cv::Point(53, 993)  , kThreshold, cv::Point(10, 10)},
    {"122", cv::Point(51, 976), kThreshold, kVoidPoint, 500}, // 時間が原因ではない??
    {"123", cv::Point(116, 1011)},
    {"100", cv::Point(648, 1128)},
    {"101", cv::Point(179, 665) },
    {"118", cv::Point(52, 975) , kThreshold, kVoidPoint, 300},
    {"124", cv::Point(122, 975) },
    {"118", cv::Point(52, 975)  },
    {"125", cv::Point(529, 425) },
    {"110", cv::Point(53, 993)  },
    {"110", cv::Point(53, 993)  },
    {"118", cv::Point(52, 975)  },
    {"126", cv::Point(102, 961) },
    {"127", cv::Point(81, 612)  },
    {"108", cv::Point(659, 1113)},
    {"109", cv::Point(211, 708) },
    {"128", cv::Point(53, 1031) },
    {"129", cv::Point(200, 613) },
    {"130", cv::Point(82, 594)  },
    {"130", cv::Point(82, 594)  },
    {"130", cv::Point(82, 594)  },
    {"110", cv::Point(54, 993)  },
    {"111", cv::Point(260, 996) , kThreshold, kVoidPoint, 500},
    {"111", cv::Point(325, 996) },
    {"100", cv::Point(648, 1128)},
    {"101", cv::Point(179, 665) },
    {"110", cv::Point(53, 993)  , 0.1},
    {"131", cv::Point(123, 676) },
    {"132", cv::Point(302, 401), kThreshold, cv::Point(69, 1029) },
    {"133_a", cv::Point(156, 44)},
    {"133_a", cv::Point(156, 44)},
    {"134", cv::Point(92, 611) , kThreshold, cv::Point(580, 1197)}, // done スカ対策
    {"135", cv::Point(224, 704) },
    {"134", cv::Point(227, 611) },
    {"100", cv::Point(648, 1128)},
    {"101", cv::Point(179, 665) },
    {"137", cv::Point(655, 1212), 10e-4},
    {"130", cv::Point(223, 594) },
    {"130", cv::Point(110, 594) , kThreshold, kVoidPoint, 500},
    {"123", cv::Point(236, 712) , 0.05, cv::Point(135, 1094)}, // プレゼント無反応対策
    {"134", cv::Point(197, 611) },
    {"200", cv::Point(4, 916)   },
    {"110", cv::Point(55, 1006) , 0.05, kVoidPoint, 300},
    {"110", cv::Point(55, 1006) , 0.05, kVoidPoint, 300},
    {"110", cv::Point(55, 1006) , 0.05, kVoidPoint, 300},
    {"110", cv::Point(55, 1006) , 0.05, kVoidPoint, 300},
    {"110", cv::Point(55, 1006) , 0.05, kVoidPoint, 300},
    {"130", cv::Point(57, 996)  , 0.08, kVoidPoint, 300},
    {"139", cv::Point(128, 641) },
    {"140", cv::Point(210, 687) },
    {"105", cv::Point(41, 565)  , 0.1}
  };
  Command retry_command("retry", cv::Point(209, 664));
  Command error_close("error_close", cv::Point(214, 594));// タイムアウトしました→閉じる。通常はスキップされる

  char buf[1024];
  int dump_id = 0;

  for (int i = init_point, count = sizeof(commands) / sizeof(commands[0]); i < count; ++i) {
    Command command = commands[i];
    assert(command.temp.size() > 0);
    assert(command.threshold > 0);

    std::cout << i << ". " << command.temp << std::endl;

    for (int count = 0; true; usleep(10 * 1000), ++count) {
      ssize_t len = read(0, buf, 1024);
      if (len < -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("Failed to read STDIN");
        continue;
      }
      bool dump = false;
      if (len > 0) {
        switch (buf[0]) {
        case 'n':
          goto next;
        case 'p':
          i = i - 2;
          goto next;
        case 'd':
          dump = true;
          break;
        }
      }

      cv::Mat cap = screen.Capture();
      bool hit = false;
      if (command.left_top == kVoidPoint) {
        cv::Mat result;
        cv::matchTemplate(cap, command.img, result, CV_TM_SQDIFF_NORMED);
        double min_val;
        cv::Point left_top;
        cv::minMaxLoc(result, &min_val, nullptr, &left_top);
        cap = cap(cv::Rect(left_top, command.img.size()));
        if (min_val < command.threshold) {
          hit = true;
          command.left_top = left_top;
          std::cout << "Left Top Found  " << left_top.x << ", " << left_top.y << std::endl;
        }
      } else {
        if (command.hit(cap)) {
          hit = true;
        } else if (i < count - 1
                   && command.temp != "003" // 003より前に004が画面に出ているが反応しない
                   && commands[i+1].temp != "133_a" // 確実に1回だけ
                   && commands[i+1].hit(cap)) {
          std::cout << "skip +1" << std::endl;
          i += 1;
          command = commands[i];
          hit = true;
        } else if (i > 0
                   && count > 3
                   && commands[i-1].temp != "133_a" // 確実に1回だけ
                   && (commands[i-1].temp != "137" || commands[i].temp != "130") // 137 130 130 つづきで誤発動が多い
                   && commands[i-1].hit(cap)) {
          std::cout << "back -1" << std::endl;
          i -= 1;
          command = commands[i];
          hit = true;
        } else if (command.temp == "123" && error_close.hit(cap)) {
          std::cout << "error back -1" << std::endl;
          i -= 1;
          command = error_close;
          hit = true;
        } else if (retry_command.hit(cap)) {
          std::cout << "retry" << std::endl;
          command = retry_command;
          hit = true;
        }
      }

      if (hit) {
        ev.Touch(command.left_top.x + command.img.cols/2, command.left_top.y + command.img.rows/2);
        usleep(command.sleep_ms * 1000);
        goto next;
      } else if (command.repeat_point != kVoidPoint) {
        ev.Touch(command.repeat_point.x, command.repeat_point.y);
        usleep(100 * 1000);
      }

      if (dump) {
        snprintf(buf, 1024, "dump%03d.png", ++dump_id);
        cv::imwrite(buf, cap);
      }

      if (count > 3 && command.repeat_point == kVoidPoint) {
        usleep(std::min(count * 100, 5000) * 1000);
      }
    }
  next:
    ;
  }
  return 0;
}

bool run_command(const std::string& command) {
  int ret = system(command.c_str());
  if (ret != 0) {
    std::cerr << command << " exited with " << ret << std::endl;
    return false;
  }
  if (WIFSIGNALED(ret) &&
      (WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT))
    return false;
  return true;
}

int main(int argc, char** argv) {
  Screen screen;
  EventDevice ev;

  // make stdin nonblocking
  int flag = fcntl(0, F_GETFL);
  if (flag == -1) {
    perror("error getting flags");
    return 1;
  }
  flag = fcntl(0, F_SETFL, flag | O_NONBLOCK);
  if (flag == -1) {
    perror("error getting flags");
    return 1;
  }

  int init_point = 0;
  if (argc > 1) {
    init_point = atoi(argv[1]);
  }
  char buf[1024];
  while (1) {
    if (run(screen, ev, init_point)) {
      return 1;
    }
    sleep(10);
    time_t t = time(nullptr);
    cv::Mat cap = screen.Capture();
    snprintf(buf, 1024, "/sdcard/result%ld.png", t);
    cv::imwrite(buf, cap);
    if (!run_command("am force-stop jp.co.bandainamcoent.BNEI0242"))
      return 1;
    snprintf(buf, 1024, "cp /data/data/jp.co.bandainamcoent.BNEI0242/shared_prefs/jp.co.bandainamcoent.BNEI0242.xml /sdcard/data%ld.xml", t);
    if (!run_command(std::string(buf)))
      return 1;
    if (!run_command("rm /data/data/jp.co.bandainamcoent.BNEI0242/shared_prefs/jp.co.bandainamcoent.BNEI0242.xml"))
      return 1;
    if (!run_command("monkey -p jp.co.bandainamcoent.BNEI0242 -c android.intent.category.LAUNCHER 1"))
      return 1;
    sleep(3);
  }
  return 0;
}
