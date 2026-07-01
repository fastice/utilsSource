CC =		gcc
MACHTYPE =	$(shell uname -m)
OSTYPE =	$(shell uname -s)
OBJDIR =	$(MACHTYPE)-$(OSTYPE)

$(shell mkdir -p $(OBJDIR))

CFLAGS =	$(FLAGS) -c -DPAF=$(PAF) -I$(INCLUDEPATH) -MMD -MP

SRCS =	filterfloat.c filterFloatImage.c

OBJS =	$(SRCS:%.c=$(OBJDIR)/%.o)

.KEEP_STATE:
all:	$(OBJS)

-include $(OBJS:.o=.d)

$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $< -o $@
