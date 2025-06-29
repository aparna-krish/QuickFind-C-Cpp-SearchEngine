# Copyright ©2025 Hal Perkins.  All rights reserved.  Permission is
# hereby granted to students registered for University of Washington
# CSE 333 for use solely during Spring Quarter 2025 for purposes of
# the course.  No other use, copying, distribution, or modification
# is permitted without prior written consent. Copyrights for
# third-party components of this work must be honored.  Instructors
# interested in reusing these course materials should contact the
# author.

# define the commands we will use for compilation and library building
AR = ar
ARFLAGS = rcs
CC = gcc
CXX = g++

# define useful flags to cc/ld/etc.
CFLAGS = -g -Wall -Wpedantic -std=c17 -I. -I./libhw1 -I./libhw2 -I./libhw3 -I.. -O0
CXXFLAGS = -g -Wall -Wpedantic -std=c++17 -I. -I./libhw1 -I./libhw2 -I./libhw3 -I.. -O0
LDFLAGS = -L. -L./libhw1 -L./libhw2 -L./libhw3 -lhw4 -lhw3 -lhw2 -lhw1 -lpthread
CPPUNITFLAGS = -L../gtest -lgtest

# define common dependencies
OBJS_COMMON = ThreadPool.o ServerSocket.o HttpServer.o HttpConnection.o FileReader.o
OBJS_GOOD = $(OBJS_COMMON) HttpUtils.o

HEADERS = FileReader.h HttpConnection.h HttpRequest.h HttpResponse.h \
	  HttpServer.h HttpUtils.h ServerSocket.h ThreadPool.h

TESTOBJS = test_serversocket.o test_threadpool.o test_filereader.o \
	   test_httpconnection.o test_httputils.o test_suite.o

all: http333d test_suite

http333d: http333d.o libhw4.a $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ http333d.o libhw4.a $(LDFLAGS)

libhw4.a: $(OBJS_GOOD) $(HEADERS)
	$(AR) $(ARFLAGS) $@ $(OBJS_GOOD)

test_suite: $(TESTOBJS) libhw4.a $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(TESTOBJS) \
	$(CPPUNITFLAGS) $(LDFLAGS)

%.o: %.cc $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $<

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<

clean:
	/bin/rm -f *.o *~ test_suite http333d libhw4.a
