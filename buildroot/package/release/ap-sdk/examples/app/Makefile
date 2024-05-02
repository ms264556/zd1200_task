TARGET=hello

${TARGET}:
	@mips-linux-gcc hello.c -o $@
	@cp -v $@ ../../linux/rootfs/usr/bin

clean:
	@rm `pwd`/$(TARGET)
	@rm ../../linux/rootfs/usr/bin/$(TARGET) 
