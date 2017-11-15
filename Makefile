MYSQLLIBDIR = /usr/local/lib/mysql
MYSQLINCDIR = /usr/local/include/mysql

TCLLIBDIR = /usr/local/lib
TCLINCDIR = /usr/local/include/tcl8.6

#CC = gcc
PIC = -fPIC
INCLUDES = -I/home/local/include -I$(MYSQLINCDIR) -I$(TCLINCDIR)
CFLAGS += -Wall $(PIC) $(INCLUDES)

# For Solaris8 :
LDFLAGS =  -lz -G -L $(MYSQLLIBDIR) -L $(TCLLIBDIR) -lmysqlclient

all:    fbsql.so

fbsql.so: fbsql.o
	$(LD) -o $@  $<  $(LDFLAGS)

clean:
	-rm -f core *.o

clobber: clean
	-rm -f fbsql.so
