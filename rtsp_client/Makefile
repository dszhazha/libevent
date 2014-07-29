# Hisilicon Hi3516 sample Makefile

#include ./Makefile.param
#ifeq ($(SAMPLE_PARAM_FILE), )
#     SAMPLE_PARAM_FILE:=../Makefile.param
#     include $(SAMPLE_PARAM_FILE)
#endif

# target source

#TOOL_CHAIN_DIR=../../gcc/arm-hisiv100-linux/target/bin/

#export CROSS_COMPILE?= ${TOOL_CHAIN_DIR}/arm-hisiv100nptl-linux-
#export CROSS?= ${TOOL_CHAIN_DIR}/arm-hisiv100nptl-linux-

export CC:= gcc
export AR:= ar
export STRIP:= strip


SRC  := $(wildcard src/*.c) 
OBJ  := $(SRC:src/%.c=release/%.o)
HEAD := $(wildcard include/*.h)

TARGET := rtsp_client
.PHONY : clean all

all: $(TARGET)

AUDIO_LIBA:=
MPI_LIBS := 
EX_LIBS := 
CFLAGS := -Wall -g -I ./include -L ./lib -levent

$(TARGET): ${OBJ}
	$(CC) $^ $(CFLAGS) $(MPI_LIBS) $(AUDIO_LIBA) $(EX_LIBS)  -o $@ 
	$(STRIP) $(TARGET)

${OBJ}: release/%.o : src/%.c ${HEAD}
	@mkdir -p release
	${CC} $(CFLAGS) -c -o $@ $<

clean:
	@rm -rf release
	@rm -f $(TARGET)
	@rm -f $(OBJ)

cleanstream:
	@rm -f *.h264
	@rm -f *.jpg
	@rm -f *.mjp
	@rm -f *.mp4
