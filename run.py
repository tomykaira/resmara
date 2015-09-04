import sys
sys.path.append('/home/tomita/local/lib/python2.7/site-packages')

import cv2
import glob
import math
import numpy as np
import subprocess
import time

adb = '/large/opt/adt-bundle-linux-x86-20131030/sdk/platform-tools/adb'

def parse_uint(arr):
    return (ord(arr[3]) << 24) | (ord(arr[2]) << 16) | (ord(arr[1]) << 8) | ord(arr[0])

def grab_screen():
    p = subprocess.Popen([adb, "shell", "screencap"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    data, stderr_data = p.communicate()
    data = data.replace("\r\n", "\n")
    if stderr_data != '':
        print stderr_data
    rows = parse_uint(data[0:4])
    cols = parse_uint(data[4:8])
    depth = parse_uint(data[8:12])
    channels = 4
    assert depth == 1
    f = np.fromstring(data[12:], dtype=np.uint8,count=rows*cols*channels)
    img = f.reshape((cols, rows, channels))
    return cv2.cvtColor(img, cv2.COLOR_BGR2RGB)

id = 0
def touch(x, y):
    global id
    print(x, y)
    subprocess.call([adb, "shell",
                     """
                     sendevent /dev/input/event2 3 57 %d
                     sendevent /dev/input/event2 3 48 10
                     sendevent /dev/input/event2 3 58 29
                     sendevent /dev/input/event2 3 53 %d
                     sendevent /dev/input/event2 3 54 %d
                     sendevent /dev/input/event2 0 0 0
                     sendevent /dev/input/event2 3 57 4294967295
                     sendevent /dev/input/event2 0 0 0
                     """ %
                     (id, x, y)])
    id += 1


def run():
    for temp, x, y in [
            ("000", -1, -1),
            ("001", 360, 640),
            ("002", -1, -1),
            ("003", -1, -1),
            ("004", -1, -1),
            ("100", -1, -1),
            ("101", -1, -1),
            ("102", -1, -1),
            ("102", 0.01, 500),
            ("100", -1, -1),
            ("101", -1, -1),
            ("104", -1, -1),
            ("105", -1, -1),
            ("106", -1, -1),
            ("107", -1, -1),
            ("100", -1, -1),
            ("101", -1, -1),
            ("108", -1, -1),
            ("109", -1, -1),
            #("111", 69, 1029), # ???
            ("115", 69, 1029),
            ("116", -1, -1),
            ("117", -1, -1),
            ("100", -1, -1),
            ("101", -1, -1),
            ("118", -1, -1),
            ("119", -1, -1),
            ("120", -1, -1),
            # ("121", 10, 10), # ???
            ("110", 10, 10),
            ("122", -1, -1),
            ("123", -1, -1),
            ("100", -1, -1),
            ("101", -1, -1),
            ("118", -1, -1),
            ("124", -1, -1),
            ("118", -1, -1),
            ("125", -1, -1),
            ("110", -1, -1),
            ("110", -1, -1),
            ("118", -1, -1),
            ("126", -1, -1),
            ("127", 0.03, -1),
            ("108", -1, -1),
            ("109", -1, -1),
            ("128", -1, -1),
            ("129", -1, -1),
            ("130", -1, -1),
            ("130", -1, -1),
            ("130", -1, -1),
            ("110", -1, -1),
            ("111", -1, -1),
            ("111", 0.01, -1),
            ("100", -1, -1),
            ("101", -1, -1),
            ("110", 0.1, -1),
            ("131", -1, -1),
            ("100", 69, 1029),
            ("101", -1, -1),
            ("132", -1, -1),
            ("133_a", -1, -1),
            ("133_a", -1, -1),
            ("133_done", -1, -1),
            ("134", -1, -1),
            ("135", -1, -1),
            ("134", -1, -1),
            ("100", -1, -1),
            ("101", -1, -1),
            ("137", 10e-4, -1),
            ("130", -1, -1),
            ("130", -1, -1),
            ("138", -1, -1), # present
            ("201", -1, -1),
            ("123", 0.05, -1),
            ("134", -1, -1),
            ("200", -1, -1),
            ("110", 0.05, -1),
            ("110", 0.05, -1),
            ("110", 0.05, -1),
            ("110", 0.05, -1),
            ("110", 0.05, -1),
            ("130", 0.08, -1),
            ("139", -1, -1),
            ("140", -1, -1),
            ("105", 0.1, -1),
    ]:
        thres = 1e-2
        if x > 0 and x < 1:
            thres = x
            x = -1
        if type(x) == float:
            thres = x - math.floor(x)
            x = int(x)
        sleep = 1
        if y > 0 and x < 0:
            sleep = y
            y = -1
        print(temp, thres, x, y)
        template = cv2.imread("templates/" + temp + ".png")
        h, w = template.shape[:-1]
        count = 0
        while cv2.waitKey(1) & 0xFF != 27:
            img = grab_screen()
            result = cv2.matchTemplate(img, template, cv2.TM_SQDIFF_NORMED)
            min_val, max_val, min_loc, max_loc = cv2.minMaxLoc(result)
            found = min_val < thres
            print(min_val, found)
            top_left = min_loc
            bottom_right = (top_left[0] + w, top_left[1] + h)
            cv2.rectangle(img, top_left, bottom_right, (0, 0, 255) if found else (255, 0, 0), 2)
            cv2.imshow('detected', img)

            if found:
                cv2.waitKey(sleep)
                touch(top_left[0] + w/2, top_left[1] + h/2)
                break
            elif x >= 0 and y >= 0:
                for i in range(5):
                    touch(x, y)
            count += 1
            if count > 3:
                time.sleep(3)

img = None
idx = 300

ix,iy = -1,-1
def save_roi(event,x,y,flags,param):
    global ix, iy, idx

    if event == cv2.EVENT_LBUTTONDOWN:
        ix,iy = x,y
    elif event == cv2.EVENT_LBUTTONUP:
        if ix > x:
            t = ix
            ix = x
            x = t
        if iy > y:
            t = iy
            iy = y
            y = t
        print(ix, x, iy, y)
        cv2.imwrite("templates/%03d.png" % idx, img[iy:y, ix:x])
        idx += 1
        #touch((ix + x)/2, (iy + y)/2)

def capture():
    global img
    while cv2.waitKey(0) & 0xFF != 27:
        img = grab_screen()
        cv2.namedWindow('preview')
        cv2.setMouseCallback('preview', save_roi)
        cv2.imshow('preview', img)

capture()
cv2.destroyAllWindows()
