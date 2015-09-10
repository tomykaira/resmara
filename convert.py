import sys
sys.path.append('/home/tomita/local/lib/python2.7/site-packages')

import glob
import numpy as np
import cv2

def parse_uint(arr):
    return (ord(arr[3]) << 24) | (ord(arr[2]) << 16) | (ord(arr[1]) << 8) | ord(arr[0])

img = None
template_name = None

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
        print(ix, x, iy, y)
        #cv2.imwrite(template_name, img[iy:y, ix:x])

for file in glob.glob('src_images/201*.raw'):
    template_name = "templates/128.png"
    with open(file, 'rb') as fd:
        rows = parse_uint(fd.read(4))
        cols = parse_uint(fd.read(4))
        depth = parse_uint(fd.read(4))
        channels = 4
        assert depth == 1
        f = np.fromfile(fd, dtype=np.uint8,count=rows*cols*channels)
        img = f.reshape((cols, rows, channels))
        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGBA)
        cv2.namedWindow('preview')
        cv2.setMouseCallback('preview', save_roi)
        while cv2.waitKey(1) & 0xFF != 27:
            cv2.imshow('preview', img)
        cv2.destroyAllWindows()
