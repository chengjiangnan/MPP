# the target that is built when you run "make" with no arguments:
default: all

MODULES = \
        mpp_adv_ingress \
        mpp_adv_egress \
        mpp_controller \
        mpp_dataplane_proxy \
        tcp_client \
        tcp_server \
        mpp_decoder

SRCS = utils \
       output_log \
       subflow \
       mpp_header \
       mpp_ctrl_header \
       mpp_source \
       mpp_sink \
       mpp_conn \
       mpp_proxy \
       mpp_proxy_conn \
       mpp_gateway

# Make all files described in MODULES
all: $(MODULES)

# running "make clean" will remove all files ignored by git.
# To ignore more files, you should add them to the file .gitignore
clean:
	@$(RM) -rf module/*.o src/*.o bin/

################################################################################
# Everything below this line can be safely ignored.

CC     = gcc
CFLAGS = -Iinclude -pthread -g -ggdb -O0

SRCS_OBJ = $(patsubst %,src/%.o,$(SRCS))

%: module/%.o $(SRCS_OBJ)
	mkdir -p bin
	$(CC) $(CFLAGS) -o bin/$@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: default all clean
