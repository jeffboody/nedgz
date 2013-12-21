# don't include LOCAL_PATH for submodules

include $(CLEAR_VARS)
LOCAL_MODULE    := nedgz
LOCAL_CFLAGS    := -Wall
LOCAL_SRC_FILES := nedgz/nedgz_tile.c nedgz/nedgz_log.c nedgz/nedgz_scene.c nedgz/nedgz_util.c

LOCAL_LDLIBS    := -Llibs/armeabi \
                   -llog -lz

include $(BUILD_SHARED_LIBRARY)
