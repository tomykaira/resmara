#host
 /large/opt/android-ndk-r10e/ndk-build && adb push libs/armeabi-v7a/res_mara /sdcard/res_mara

# android
mount -ro remount,rw /system
pkill res_mara
cd /sdcard && cp /sdcard/res_mara /system/bin/res_mara && nohup res_mara 0 &
