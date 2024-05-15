# build an executable named myprog from myprog.c
all: serverM.cpp serverS.cpp serverD.cpp serverU.cpp client.cpp
	g++ -g -Wall -o serverM serverM.cpp
	g++ -g -Wall -o serverS serverS.cpp
	g++ -g -Wall -o serverD serverD.cpp
	g++ -g -Wall -o serverU serverU.cpp
	g++ -g -Wall -o client client.cpp

  clean: 
	$(RM) myprog serverM serverS serverD serverU client