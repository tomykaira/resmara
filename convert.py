import sys
sys.path.append('/home/tomita/local/lib/python2.7/site-packages')

import glob
import numpy as np
import cv2
import commands
import datetime

def parse_uint(arr):
    return (ord(arr[3]) << 24) | (ord(arr[2]) << 16) | (ord(arr[1]) << 8) | ord(arr[0])

png = commands.getoutput('adb shell screencap -p | perl -pe \'s/\\x0D\\x0A/\\x0A/g\'')
with open('test.png', 'wb') as f:
    f.write(png)
img = cv2.imdecode(np.fromstring(png, dtype=np.uint8), 1)
template_name = sys.argv[1]

ix,iy = -1,-1
def save_roi(event,x,y,flags,param):
    global ix, iy

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
        print(ix, iy)
        cv2.imwrite('templates/' + template_name + '.png', img[iy:y, ix:x])

cv2.namedWindow('preview')
cv2.setMouseCallback('preview', save_roi)
while cv2.waitKey(1) & 0xFF != 27:
    cv2.imshow('preview', img)
cv2.destroyAllWindows()
