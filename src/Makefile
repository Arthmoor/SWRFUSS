CC      = gcc
PROF    = 

#Uncomment to compile in Cygwin
#CYGWIN = -DCYGWIN

#Uncomment the line below if you are getting warnings about undefined math functions
NEED_MATH = -lm

L_FLAGS = $(PROF) $(NEED_MATH) -lz

#IMC2 - Comment out to disable IMC2 support
IMC = 1

W_FLAGS = -Wall -Werror -Wshadow -Wformat-security -Winline -Wpointer-arith -Wcast-align -Wredundant-decls -Wstrict-prototypes

C_FLAGS = -g2 $(W_FLAGS) $(PROF)

C_FILES = act_comm.c act_info.c act_move.c act_obj.c act_wiz.c boards.c bounty.c \
	  build.c clans.c color.c comm.c comments.c const.c db.c fight.c \
	  handler.c hashstr.c hotboot.c interp.c magic.c makeobjs.c mccp.c \
	  md5.c misc.c mud_comm.c mud_prog.c player.c reset.c save.c shops.c \
	  skills.c space.c special.c swskills.c tables.c track.c update.c

ifdef IMC
   C_FILES := imc.c $(C_FILES)
   C_FLAGS := $(C_FLAGS) -DIMC -DIMCSMAUG
endif

O_FILES := $(patsubst %.c,o/%.o,$(C_FILES))

H_FILES = $(wildcard *.h) 

all:
	$(MAKE) -s swreality

ifdef CYGWIN
swreality: $(O_FILES)
	rm -f swreality.exe
	$(CC) -o swreality.exe $(O_FILES) $(L_FLAGS)
	echo "Done compiling mud.";
	chmod g+w swreality.exe
	chmod a+x swreality.exe
	chmod g+w $(O_FILES)
else
swreality: $(O_FILES)
	rm -f swreality
	$(CC) -o swreality $(O_FILES) $(L_FLAGS)
	echo "Done compiling mud.";
	chmod g+w swreality
	chmod g+w $(O_FILES)
endif

.c.o: mud.h
	$(CC) -c $(C_FLAGS) $<

clean:
	rm -f $(O_FILES) swreality swreality.exe

indent:
	indent -ts3 -nut -nsaf -nsai -nsaw -npcs -npsl -ncs -nbc -bls -prs -bap -cbi0 -cli3 -bli0 -l125 -lp -i3 -cdb -c1 -cd1 -sc -pmt $(C_FILES)
	indent -ts3 -nut -nsaf -nsai -nsaw -npcs -npsl -ncs -nbc -bls -prs -bap -cbi0 -cli3 -bli0 -l125 -lp -i3 -cdb -c1 -cd1 -sc -pmt $(H_FILES)

indentclean:
	rm *.c~ *.h~

o/%.o: %.c
	echo "  Compiling $@....";
	$(CC) -c $(C_FLAGS) $< -o $@
