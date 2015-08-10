CC=g++
CFLAGS=-W -Wall -ansi -pedantic -std=c++11
LDFLAGS=-lavformat -lavcodec
OBJECTS=main.o LocalMusicSource.o
EXEC=main

$(EXEC): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@  $^ $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $^ $(LDFLAGS)

clean:
	rm *.o *~ \#*

cleaner:
	rm *.o *~ \#* $(EXEC)
