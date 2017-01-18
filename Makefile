OBJECTS := labwork8.o 
CC := gcc

all: labwork8

labwork8: $(OBJECTS)
	$(CC) $(OBJECTS) -lpthread -o labwork8

%.o: %.c
	$(CC) -c -g $< -o $@

clean:
	rm -f *.o
