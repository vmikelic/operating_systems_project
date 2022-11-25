all: 
	gcc GroupProject.c -lm -lpthread -lrt -o GroupProject
clean:
	rm GroupProject