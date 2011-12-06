all: hwdc2

test: drv_test

drv_test_hwd.h: drv_test_hwd.hwd hwdc2
	./hwdc2 $*.hwd $@

drv_test: drv_test.c drv_test_hwd.h
	gcc -Wall -Werror -o drv_test drv_test.c

hwdc2: hwdc2.cpp ptr.hpp
	g++ -Wall -Werror -o hwdc2 hwdc2.cpp

