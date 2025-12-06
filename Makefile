hello:
	gcc mines.c -g -std=c23 -fsanitize=address -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o mines
