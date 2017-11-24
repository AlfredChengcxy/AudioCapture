all :tinyplay tinypcminfo tinycap tinymix 
.PHONY : clean
tinyplay:tinyplay.o pcm.o
	arm-none-linux-gnueabi-gcc -o tinyplay tinyplay.o pcm.o
tinypcminfo:tinypcminfo.o pcm.o
	arm-none-linux-gnueabi-gcc -o tinypcminfo tinypcminfo.o pcm.o  
tinycap:tinycap.o pcm.o
	arm-none-linux-gnueabi-gcc -o tinycap tinycap.o pcm.o
tinymix:tinymix.o mixer.o
	arm-none-linux-gnueabi-gcc -o tinymix tinymix.o mixer.o
tinyplay.o:tinyplay.c
	arm-none-linux-gnueabi-gcc -c tinyplay.c
tinypcminfo.o:tinypcminfo.c
	arm-none-linux-gnueabi-gcc -c tinypcminfo.c
tinycap.o:tinycap.c
	arm-none-linux-gnueabi-gcc -c tinycap.c
tinymix.o:tinymix.c
	arm-none-linux-gnueabi-gcc -c tinymix.c
pcm.o:pcm.c
	arm-none-linux-gnueabi-gcc -c pcm.c
mixer.o:mixer.c
	arm-none-linux-gnueabi-gcc -c mixer.c
#clean:
#	rm mixer.o pcm.o tinymix.o tinycap.o tinypcminfo.o tinyplay.o
