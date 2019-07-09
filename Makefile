all: mirror_client sender receiver

mirror_client : client.o mirrorImplementation.o
	gcc client.o mirrorImplementation.o -o mirror_client -g `gpgme-config --libs`

sender: sender.o mirrorImplementation.o client.o
	gcc sender.o -o sender mirrorImplementation.o -lm -g `gpgme-config --libs`

receiver: receiver.o mirrorImplementation.o
	gcc receiver.o -o receiver mirrorImplementation.o -lm -g `gpgme-config --libs`

client.o : client.c mirrorInterface.h mirrorTypes.h
	gcc -c client.c -g `gpgme-config --cflags`

mirrorImplementation.o : mirrorImplementation.c mirrorInterface.h mirrorTypes.h
	gcc -c mirrorImplementation.c -g `gpgme-config --cflags`

sender.o: sender.c
	gcc -c sender.c -g `gpgme-config --cflags`

receiver.o: receiver.c
	gcc -c receiver.c -g `gpgme-config --cflags`

clean:
	rm mirror_client client.o mirrorImplementation.o sender sender.o receiver receiver.o
