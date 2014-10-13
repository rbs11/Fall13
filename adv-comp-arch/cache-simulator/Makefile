CXXFLAGS := -g -Wall -lm 
#-I/usr/include/glib-2.0 -I/usr/lib/i386-linux-gnu/glib-2.0/include  -lglib-2.0 -lgmodule-2.0 -lgobject-2.0
#$(shell pkg-config --libs --cflags glib-2.0)# $(shell pkg-config --libs --cflags gobject-2.0)

# glib-2.0) 
CXX=g++

all: cachesim

cachesim: cachesim.o cachesim_driver.o
	$(CXX) -o cachesim cachesim.o cachesim_driver.o

clean:
	rm -f cachesim *.o
