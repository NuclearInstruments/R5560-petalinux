APP = nibridge

all: build install

build: 

clean:

.PHONY: install image

install: $(APP)
 $(TARGETINST) -d -p 0755 mystartup /etc/init.d/mystartup 
 $(TARGETINST) -s /etc/init.d/mystartup /etc/rc5.d/S99mystartup 
