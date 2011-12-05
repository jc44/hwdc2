all: hwdc2

hwdc2: hwdc2.cpp ptr.hpp
	g++ -Wall -Werror -o hwdc2 hwdc2.cpp

