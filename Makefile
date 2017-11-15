# path to libmysqlclient.a :
MYSQLLIBDIR = /usr/lib/mysql
# path to header files of mysql:
MYSQLINCDIR = /usr/include/mysql
# don´t forget mysql-3.22.26a.tar.gz for example from www.sunfreeware.com :
MYSQLINCDIR2 = /home/users/wbo/projects/Tools/mysql-3.22.26a/include
# a different compiler :
CC = gcc
PIC = -fPIC
INCLUDES = -I/home/local/include -I$(MYSQLINCDIR) -I$(MYSQLINCDIR2)
CFLAGS += -Wall $(PIC) $(INCLUDES)

# For Solaris8 :
LDFLAGS =  -lz -G -L $(MYSQLLIBDIR) -l mysqlclient

all:    fbsql.so 

fbsql.so: fbsql.o
	$(LD) -o $@  $<  $(LDFLAGS)

clean:
	-rm -f core *.o

clobber: clean
	-rm -f fbsql.so
