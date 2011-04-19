P_NAME          :=  libclibutils.a
P_C_SRCS        :=  $(wildcard *.c)
P_C_OBJS        :=  ${P_C_SRCS:.c=.o}
P_OBJS          :=  $(P_C_OBJS) 
P_INCLUDE_DIRS  :=  ../inc
CPPFLAGS        +=  $(foreach includedir,$(P_INCLUDE_DIRS),-I$(includedir))
LDFLAGS         +=  $(foreach librarydir,$(P_LIBRARY_DIRS),-L$(librarydir))
LDFLAGS         +=  $(foreach library,$(P_LIBRARIES),-l$(library))
CXX             :=  gcc
CC              :=  gcc -g -Wall -Wextra -Wparentheses -pedantic

.PHONY:         all clean
all:            $(P_NAME)
$(P_NAME):      $(P_OBJS)
		ar rcs $(P_NAME) $(P_OBJS)
clean:
		@- $(RM) $(P_NAME)
		@- $(RM) $(P_OBJS)
		@- $(RM) core*
		@- $(RM) tags

