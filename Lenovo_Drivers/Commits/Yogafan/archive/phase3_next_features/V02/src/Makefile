obj-m += yogafan.o

KVERSION = 7.1.1-yoga-sml-amd64
CUR_DIR  := $(shell pwd)

all:
	$(MAKE) -C "/lib/modules/$(KVERSION)/build" M="$(CUR_DIR)" modules

clean:
	$(MAKE) -C "/lib/modules/$(KVERSION)/build" M="$(CUR_DIR)" clean
