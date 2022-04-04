all: oss user
oss: oss.c
	gcc -o oss oss.c queue.c
user: user.c
	gcc -o user user.c
clean:
	-rm oss user output_log.txt
