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

const double kThreshold = 0.01;
const cv::Point kVoidPoint(-1, -1);
const int kSleepMs = 1;

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
    // {"000", cv::Point(233, 402)},
    // {"001", cv::Point(366, 428), kThreshold, cv::Point(360, 640)},
    // {"002", cv::Point(56, 309)},
    // {"003", cv::Point(55, 311)},
    // {"004", cv::Point(148, 673)},
    // {"100", cv::Point(648, 1128)},
    // {"101", cv::Point(179, 665)},
    // {"102", cv::Point(74, 845), kThreshold, kVoidPoint, 500},
    {"102", kThreshold, kVoidPoint, 500},
    {"100"},
    {"101"},
    {"104"},
    {"105"},
    {"106"},
    {"107"},
    {"100"},
    {"101"},
    {"108"},
    {"109"},
    {"115", kThreshold, cv::Point(69, 1029)},
    {"116"},
    {"117"},
    {"100"},
    {"101"},
    {"118"},
    {"119"},
    {"120"},
    {"110", kThreshold, cv::Point(10, 10)},
    {"122"},
    {"123"},
    {"100"},
    {"101"},
    {"118"},
    {"124"},
    {"118"},
    {"125"},
    {"110"},
    {"110"},
    {"118"},
    {"126"},
    {"127", 0.03},
    {"108"},
    {"109"},
    {"128"},
    {"129"},
    {"130"},
    {"130"},
    {"130"},
    {"110"},
    {"111"},
    {"111"},
    {"100"},
    {"101"},
    {"110", 0.1},
    {"131"},
    {"100", kThreshold, cv::Point(69, 1029)},
    {"101"},
    {"132"},
    {"133_a"},
    {"133_a"},
    {"133_done"},
    {"134"},
    {"135"},
    {"134"},
    {"100"},
    {"101"},
    {"137", 10e-4},
    {"130"},
    {"130"},
    {"138"},
    {"201"},
    {"123", 0.05},
    {"134"},
    {"200"},
    {"110", 0.05},
    {"110", 0.05},
    {"110", 0.05},
    {"110", 0.05},
    {"110", 0.05},
    {"130", 0.08},
    {"139"},
    {"140"},
    {"105", 0.1}
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
      cv::Mat result;
      cv::matchTemplate(cap(cv::Rect(command.left_top, cv::Size(w, h))), tmpl_img, result, CV_TM_SQDIFF_NORMED);
      double min_val;
      cv::Point top_left;
      cv::minMaxLoc(result, &min_val, nullptr, &top_left);
      std::cout << min_val << ", " << command.threshold << std::endl;

      if (min_val < command.threshold) {
        std::cout << ">>>>>> {\"" << command.temp << "\", cv::Point(" << top_left.x << ", " << top_left.y << "), " << std::endl;
        ev.Touch(top_left.x + w/2, top_left.y + h/2);
        goto next;
      } else if (command.repeat_point != kVoidPoint) {
        for (int i = 0; i < 5; ++i) {
          ev.Touch(command.repeat_point.x, command.repeat_point.y);
          usleep(100 * 1000);
        }
      }

      if (dump) {
        cv::Point bottom_right(top_left.x + w, top_left.y + h);
        cv::rectangle(cap, top_left, bottom_right, min_val < command.threshold ? cv::Scalar(0, 0, 255) : cv::Scalar(255, 0, 0), 2);
        snprintf(buf, 1024, "dump%03d.png", ++dump_id);
        cv::imwrite(buf, cap);
      }

      if (count > 3) {
        usleep(std::min(count * 100, 5000) * 1000);
      }
    }
  next:
    ;
  }
}
