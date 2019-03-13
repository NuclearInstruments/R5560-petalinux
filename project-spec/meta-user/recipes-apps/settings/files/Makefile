include apps.common.mk

APP = settings

all: build install

.PHONY: build
build:

install:
	# Copy settings files to the rootfs.
	cp interfaces $(TARGETDIR)/etc/network/interfaces

clean:
