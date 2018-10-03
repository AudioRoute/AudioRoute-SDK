all:
	-javah -classpath bin/classes -o jni/lowpass.h \
		com.ntrack.audioroute.lowpass.SimpleSynthModule
	env NDK_MODULE_PATH=$(CURDIR)/.. ndk-build
