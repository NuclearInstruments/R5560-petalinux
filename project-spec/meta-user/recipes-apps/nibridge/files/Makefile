APP = nibridge

# Add any other object files to this list below
APP_OBJS = nibridge.o

all: build

build: $(APP)

$(APP): $(APP_OBJS)
	$(CC) $(LDFLAGS)  -o $@ $(APP_OBJS) $(LDLIBS) -lpthread libzmq.so.5.2.2

