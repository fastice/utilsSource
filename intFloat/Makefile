CC =		gcc
MACHTYPE =	$(shell uname -m)
OSTYPE =	$(shell uname -s)
OBJDIR =	$(MACHTYPE)-$(OSTYPE)

$(shell mkdir -p $(OBJDIR))

CFLAGS =	$(FLAGS) -c -DPAF=$(PAF) -I$(INCLUDEPATH) -MMD -MP

SRCS =	intfloat.c intFloatImage.c getPhaseImage.c vrtIO.c

OBJS =	$(SRCS:%.c=$(OBJDIR)/%.o)

-include $(OBJS:.o=.d)

.KEEP_STATE:
all:	$(OBJS)

$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $< -o $@
