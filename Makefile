TOP_DIR = .
INC_DIR = $(TOP_DIR)/inc
SRC_DIR = $(TOP_DIR)/src
BUILD_DIR = $(TOP_DIR)/build

CC=gcc
FLAGS = -pthread -g -ggdb -DDEBUG -I$(INC_DIR) -Wall
OBJS = $(BUILD_DIR)/tju_packet.o \
	   $(BUILD_DIR)/logger.o \
	   $(BUILD_DIR)/kernel.o \
	   $(BUILD_DIR)/trans.o \
	   $(BUILD_DIR)/queue.o \
	   $(BUILD_DIR)/list.o \
	   $(BUILD_DIR)/util.o \
	   $(BUILD_DIR)/timer_list.o \
	   $(BUILD_DIR)/tju_tcp.o \



default:all

all: clean server client

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c 
	$(CC) $(FLAGS) -c -o $@ $<

clean:
	find . -type f | xargs -n 5 touch
	-rm -f ./build/*.o client server

server: $(OBJS)
	$(CC) $(FLAGS) ./src/server.c -o server $(OBJS)

client:
	$(CC) $(FLAGS) ./src/client.c -o client $(OBJS) 



	
