test_prg: test_prg.cpp libmap.a goldchase.h libmap.a
	g++ -std=c++17 test_prg.cpp -o test_prg -L. -lmap -lpanel -lncurses

libmap.a: Screen.o Map.o
	ar -r libmap.a Screen.o Map.o

Screen.o: Screen.cpp
	g++ -std=c++17 -c Screen.cpp

Map.o: Map.cpp
	g++ -std=c++17 -c Map.cpp

clean:
	rm -f Screen.o Map.o libmap.a test_prg
