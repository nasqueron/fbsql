MYSQL_INCLUDES != mysql_config --include
MYSQL_LIBS != mysql_config --libs

TCLLIBDIR = /usr/local/lib
TCLINCDIR = /usr/local/include/tcl8.6

SYSLIBDIR = /usr/lib

PIC = -fPIC
INCLUDES = $(MYSQL_INCLUDES) -I$(TCLINCDIR)
CFLAGS += -Wall $(PIC) $(INCLUDES)

LDFLAGS = -lz -shared $(MYSQL_LIBS) -L $(SYSLIBDIR) -L $(TCLLIBDIR)

all:    fbsql.so

fbsql.so: fbsql.o
	$(LD) -o fbsql.so fbsql.o $(LDFLAGS)

clean:
	-rm -f core *.o

clobber: clean
	-rm -f fbsql.so
