test:
	rm -rf build
	mkdir build
	mkdir build/objs
	gcc -c -Werror -I. -Iunity/src/ -Imock/ -Isrc/ testunit_counter_face.c -o build/objs/testunit_counter_face.o
	gcc -c -Werror -I. -Iunity/src/ -Isrc/ -DTEST unity/src/unity.c -o build/objs/unity.o
	gcc -c -Werror -Wno-format-overflow -I. -Imock/ -Iunity/src/ -Isrc/ -DTEST src/beer_counter_face.c -o build/objs/beer_counter_face.o
	gcc -c -Werror -I. -Imock/ -Iunity/src/ -Isrc/ -DTEST mock/watch.c -o build/objs/watch.o
	gcc -c -Werror -I. -Imock/ -Iunity/src/ -Isrc/ -DTEST mock/movement.c -o build/objs/movement.o
	gcc -c -Werror -I. -Imock/ -Iunity/src/ -Isrc/ -DTEST src/bac.c -o build/objs/bac.o
	gcc -o build/testunit_counter_face.out build/objs/testunit_counter_face.o build/objs/unity.o build/objs/beer_counter_face.o build/objs/watch.o build/objs/bac.o build/objs/movement.o
	./build/testunit_counter_face.out
