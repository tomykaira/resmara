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

enum Action {
  ACTION_NONE = 0,
  ACTION_SCREENSHOT
};

struct Command {
  std::string temp;
  cv::Mat img;
  cv::Point left_top;
  double threshold;
  cv::Point repeat_point;
  int sleep_ms;
  Action action;

  Command(std::string const& t, cv::Point const& lt, double h, cv::Point const& p, int s) :
    left_top(lt),
    threshold(h),
    repeat_point(p),
    sleep_ms(s),
    action(ACTION_NONE) {

    std::size_t pos = t.find("|");
    if (pos == std::string::npos) {
      temp = t;
    } else {
      temp = t.substr(0, pos);
      std::string act = t.substr(pos + 1, std::string::npos);
      if (act == "screenshot") {
        action = ACTION_SCREENSHOT;
      }
    }
    img = cv::imread("templates/" + temp + ".png");
  }

  Command(std::string const& t, cv::Point const& lt, double h, cv::Point const& p) : Command(t, lt, h, p, kSleepMs) {}
  Command(std::string const& t, cv::Point const& lt, double h) : Command(t, lt, h, kVoidPoint) {}
  Command(std::string const& t, cv::Point const& lt) : Command(t, lt, kThreshold) {}

  bool hit(cv::Mat const& cap);
};

bool Command::hit(cv::Mat const& cap) {
  cv::Rect rect(left_top, img.size());
  if (!(0 <= rect.x
        && 0 <= rect.width
        && rect.x + rect.width <= cap.cols
        && 0 <= rect.y
        && 0 <= rect.height
        && rect.y + rect.height <= cap.rows))
    return false;
  cv::Mat roi = cap(rect);
  cv::Mat result;
  cv::matchTemplate(roi, img, result, CV_TM_SQDIFF_NORMED);
  assert(result.total() == 1);
  return result.at<float>(0, 0) < threshold;
}

int run(Screen& screen, EventDevice& ev, int init_point, time_t timestamp) {
  Command commands[] = {
    {"new_game", cv::Point(277, 436), kThreshold, cv::Point(360, 640)},
    {"google_play_cancel", cv::Point(243, 782)},
    {"004", cv::Point(148, 673)},
    // {"100", cv::Point(648, 1128)},
    // {"101", cv::Point(179, 665)},
    {"102", cv::Point(74, 845), kThreshold, cv::Point(360, 640), 500},
    {"102", cv::Point(225, 703)  , kThreshold, kVoidPoint, 500},
    // {"100", cv::Point(648, 1128) }, // TODO: スカりやすい
    // {"101", cv::Point(169, 665)  },
    {"104", cv::Point(60, 951), kThreshold, cv::Point(360, 640)},
    {"105", cv::Point(41, 565)   },
    {"107", cv::Point(209, 611), kThreshold, cv::Point(662, 1210)}, // スキップをスカる対策
    // {"100", cv::Point(648, 1128) },
    // {"101", cv::Point(179, 665)  },
    {"108", cv::Point(659, 1113), 0.02, cv::Point(360, 640)},
    {"109", cv::Point(211, 708), 0.03},
    {"115", cv::Point(160, 608) , kThreshold, cv::Point(69, 1029)},
    {"116", cv::Point(50, 961)  },
    {"117", cv::Point(213, 495) },
    // {"100", cv::Point(648, 1128), kThreshold, kVoidPoint, 500}, // スキップ confirm でフリーズする
    // {"101", cv::Point(179, 665) },
    {"118", cv::Point(52, 975), kThreshold, cv::Point(360, 640)},
    {"119", cv::Point(503, 601) },
    {"120", cv::Point(83, 706)  },
    {"110", cv::Point(53, 993)  , kThreshold, cv::Point(10, 10)},
    {"122", cv::Point(51, 976), kThreshold, kVoidPoint, 500}, // 時間が原因ではない??
    {"123", cv::Point(116, 1011)},
    // {"100", cv::Point(648, 1128)},
    // {"101", cv::Point(179, 665) },
    {"118", cv::Point(52, 975), kThreshold, cv::Point(360, 640), 300},
    {"live_confirm", cv::Point(123, 999) },
    {"118", cv::Point(52, 975)  },
    {"125", cv::Point(529, 425) },
    {"110", cv::Point(53, 993)  },
    {"110", cv::Point(53, 993)  },
    {"118", cv::Point(52, 975)  },
    {"126", cv::Point(102, 961) },
    {"127", cv::Point(81, 612)  },
    {"108", cv::Point(659, 1113)},
    {"109", cv::Point(211, 708) },
    {"after_live_ok", cv::Point(54, 1129) },
    {"level_up_ok", cv::Point(166, 619) },
    {"130", cv::Point(82, 594)  },
    {"130", cv::Point(82, 594)  },
    {"130", cv::Point(82, 594)  },
    {"110", cv::Point(54, 993)  },
    {"111", cv::Point(260, 996) , kThreshold, kVoidPoint, 500},
    {"111", cv::Point(325, 996) },
    // {"100", cv::Point(648, 1128)},
    // {"101", cv::Point(179, 665) },
    {"110", cv::Point(53, 993)  , 0.1, cv::Point(360, 640)},
    {"131", cv::Point(123, 676) },
    {"132", cv::Point(302, 401), kThreshold, cv::Point(69, 1029) },
    {"133_a", cv::Point(156, 44), 0.01},
    {"133_a", cv::Point(156, 44)},
    {"133_done", cv::Point(553, 1155), 0.1},
    {"134", cv::Point(92, 611)},
    {"135", cv::Point(224, 704) },
    {"134", cv::Point(227, 611) },
    // {"100", cv::Point(648, 1128)},
    // {"101", cv::Point(179, 665) },
    {"restart_ok", cv::Point(218, 612), kThreshold, cv::Point(360, 640)},
    {"login_next", cv::Point(664, 1217), 10e-4, cv::Point(360, 640)},
    {"130", cv::Point(223, 594) },
    {"130", cv::Point(110, 594) , kThreshold, kVoidPoint, 500},
    {"123", cv::Point(236, 712) , 0.1, cv::Point(135, 1094)}, // プレゼント無反応対策
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
    {"105", cv::Point(41, 565)  , 0.1, kVoidPoint, 5000},
    {"|screenshot", kVoidPoint},
    {"gacha_again", cv::Point(210, 413), kThreshold, cv::Point(360, 640)},
    {"140", cv::Point(210, 687) },
    {"105", cv::Point(41, 565)  , 0.1, kVoidPoint, 5000},
    {"|screenshot", kVoidPoint}
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
      if (command.temp == "") {
        hit = true;
      } else if (command.left_top == kVoidPoint) {
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
                   && command.temp != "google_play_cancel"
                   && command.temp != "003" // 003より前に004が画面に出ているが反応しない
                   && commands[i+1].temp != "119" // 118 と同時に画面にでている
                   && commands[i+1].temp != "live_confirm" // 118 と同時に画面にでている
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
            i -= 1;
        } else if(count > 20) { // すこし待ってから発動
          // if (!(commands[53].temp == "132" && commands[56].temp == "133_done" && commands[57].temp == "134" && commands[58].temp == "135")) {
          //   std::cerr << "Offset around producer name input is changed. Fix them." << std::endl;
          //   throw std::runtime_error("invalid offset");
          // }
          // 名前ができないケースが多いので、入力後の位置で未入力状態だったら再試行する
          if (56 <= i && i <= 58 && commands[53].hit(cap)) { // 132: 空の名前入力画面
            std::cout << "Retry name input from " << i - 4 << std::endl;
            i = 53;
            command = commands[i];
            hit = true;
          }
        }
      }

      if (hit) {
        switch (command.action) {
        case ACTION_SCREENSHOT:
          snprintf(buf, 1024, "/sdcard/result%ld-%d.png", timestamp, i);
          std::cout << "screenshot " << buf << std::endl;
          cv::imwrite(buf, cap);
          break;
        case ACTION_NONE:
          break;
        }

        if (command.left_top != kVoidPoint) {
          int x = command.left_top.x + command.img.cols/2;
          int y = command.left_top.y + command.img.rows/2;
          std::cout << "exec " << i << "." << command.temp << " (" << x << ", " << y << ")" << std::endl;
          ev.Touch(x, y);
        }
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
    if (!run_command("pgrep jp.co.bandainamcoent.BNEI0242")) {
      if (!run_command("monkey -p jp.co.bandainamcoent.BNEI0242 -c android.intent.category.LAUNCHER 1"))
        return 1;
    }
    time_t t = time(nullptr);
    if (run(screen, ev, init_point, t)) {
      return 1;
    }
    init_point = 0;
    if (!run_command("am force-stop jp.co.bandainamcoent.BNEI0242"))
      return 1;
    snprintf(buf, 1024, "cp /data/data/jp.co.bandainamcoent.BNEI0242/shared_prefs/jp.co.bandainamcoent.BNEI0242.xml /sdcard/result%ld.xml", t);
    if (!run_command(std::string(buf)))
      return 1;
    if (!run_command("rm /data/data/jp.co.bandainamcoent.BNEI0242/shared_prefs/jp.co.bandainamcoent.BNEI0242.xml"))
      return 1;
    sleep(3);
  }
  return 0;
}
