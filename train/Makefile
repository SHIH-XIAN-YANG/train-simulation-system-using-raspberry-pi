
#DEBUG	= -g -O0
DEBUG	= -O3
CC	= gcc
INCLUDE	= -I/usr/local/include
CFLAGS	= $(DEBUG) -Wall $(INCLUDE) -Winline -pipe

LDFLAGS	= -L/usr/local/lib
LDLIBS	= -lwiringPi -lwiringPiDev -lpthread -lm -lwiringPiPca9685

# Should not alter anything below this line
###############################################################################

SRC	=   infared_sensor.c train.c

OBJ	=	$(SRC:.c=.o)

BINS=	$(SRC:.c=)

all:	$(BINS)

servo:	servo.o
	@echo [link]
	@$(CC) -o $@ servo.o $(LDFLAGS) $(LDLIBS)
	
calibrate:	calibrate.o
	@echo [link]
	@$(CC) -o $@ calibrate.o $(LDFLAGS) $(LDLIBS)
	
leds:	leds.o
	@echo [link]
	@$(CC) -o $@ leds.o $(LDFLAGS) $(LDLIBS)
	
piezo:	piezo.o
	@echo [link]
	@$(CC) -o $@ piezo.o $(LDFLAGS) $(LDLIBS)
	
reset:	reset.o
	@echo [link]
	@$(CC) -o $@ reset.o $(LDFLAGS) $(LDLIBS)
	
.c.o:
	@echo [CC] $<
	@$(CC) -c $(CFLAGS) $< -o $@

clean:
	@echo "[Clean]"
	@rm -f $(OBJ) *~ core tags $(BINS)

tags:	$(SRC)
	@echo [ctags]
	@ctags $(SRC)

depend:
	makedepend -Y $(SRC)

# DO NOT DELETE