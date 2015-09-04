#include <cstdlib>
#include <unistd.h>
#include <cassert>
#include <stdint.h>

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
const int kSleepMs = 100;

struct Command {
  std::string temp;
  cv::Point left_top;
  double threshold;
  cv::Point repeat_point;
  int sleep_ms;

  Command(std::string const& t, cv::Point const& lt, double h, cv::Point const& p, int s) : temp(t), left_top(lt), threshold(h), repeat_point(p), sleep_ms(s) {}
  Command(std::string const& t, cv::Point const& lt, double h, cv::Point const& p) : Command(t, lt, h, p, kSleepMs) {}
  Command(std::string const& t, cv::Point const& lt, double h) : Command(t, lt, h, kVoidPoint) {}
  Command(std::string const& t, cv::Point const& lt) : Command(t, lt, kThreshold) {}
};

int main() {
  Command commands[] = {
    {"000", cv::Point(233, 402), kThreshold, kVoidPoint, 1000},
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
    {"122", cv::Point(51, 976)  },
    {"123", cv::Point(116, 1011)},
    {"100", cv::Point(648, 1128)},
    {"101", cv::Point(179, 665) },
    {"118", cv::Point(52, 975)  },
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
    {"130", cv::Point(223, 594) }, // TODO: よくスキップされる
    {"130", cv::Point(110, 594) , kThreshold, kVoidPoint, 500},
    // {"201", cv::Point(133, 935) , kThreshold, },
    // タイムアウトしました→閉じるが必要な場合がよくある
    {"123", cv::Point(236, 712) , 0.05, cv::Point(135, 1094)}, // プレゼント無反応対策
    {"134", cv::Point(197, 611) },
    {"200", cv::Point(4, 916)   },
    {"110", cv::Point(55, 1006) , 0.05},
    {"110", cv::Point(55, 1006) , 0.05},
    {"110", cv::Point(55, 1006) , 0.05},
    {"110", cv::Point(55, 1006) , 0.05},
    {"110", cv::Point(55, 1006) , 0.05},
    {"130", cv::Point(57, 996)  , 0.08},
    {"139", cv::Point(128, 641) },
    {"140", cv::Point(210, 687) },
    {"105", cv::Point(41, 565)  , 0.1}
  };

  Screen screen;
  EventDevice ev;

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
  char buf[1024];
  int dump_id = 0;

  for (int i = 0, count = sizeof(commands) / sizeof(commands[0]); i < count; ++i) {
    Command command = commands[i];
    assert(command.temp.size() > 0);
    assert(command.threshold > 0);

    std::cout << i << ". " << command.temp << std::endl;

    auto tmpl_img = cv::imread("templates/" + command.temp + ".png");
    int h = tmpl_img.rows, w = tmpl_img.cols;

    bool pushed = false;
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
      double min_val;
      if (command.left_top == kVoidPoint) {
        cv::Mat result;
        cv::matchTemplate(cap, tmpl_img, result, CV_TM_SQDIFF_NORMED);
        cv::Point left_top;
        cv::minMaxLoc(result, &min_val, nullptr, &left_top);
        cap = cap(cv::Rect(left_top, cv::Size(w, h)));
        if (min_val < command.threshold) {
          command.left_top = left_top;
          std::cout << "Left Top Found  " << left_top.x << ", " << left_top.y << std::endl;
        }
      } else {
        cap = cap(cv::Rect(command.left_top, cv::Size(w, h)));
        cv::Mat result;
        cv::matchTemplate(cap, tmpl_img, result, CV_TM_SQDIFF_NORMED);
        cv::minMaxLoc(result, &min_val);
      }
      std::cout << min_val << ", " << command.threshold << std::endl;

      if (min_val < command.threshold) {
        ev.Touch(command.left_top.x + w/2, command.left_top.y + h/2);
        pushed = true;
        usleep(command.sleep_ms * 1000);
        if (commands[i + 1].temp == command.temp || command.temp == "133_a")
          goto next;
      } else if (pushed) {
        goto next;
      } else if (command.repeat_point != kVoidPoint) {
        ev.Touch(command.repeat_point.x, command.repeat_point.y);
        usleep(100 * 1000);
      }

      if (dump) {
        snprintf(buf, 1024, "dump%03d.png", ++dump_id);
        cv::imwrite(buf, cap);
      }

      if (count > 3 && !pushed && command.repeat_point == kVoidPoint) {
        usleep(std::min(count * 100, 5000) * 1000);
      }
    }
  next:
    ;
  }
}
