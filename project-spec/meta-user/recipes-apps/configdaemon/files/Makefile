APP = configdaemon

# Add any other object files to this list below
APP_OBJS = cJSON.o configdaemon.o

all: build

build: $(APP)

$(APP): $(APP_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(APP_OBJS) $(LDLIBS) -lm

