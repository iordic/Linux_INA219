main:
	gcc -Wall -o test-ina219 test-ina219.c ina219.c -li2c
clean:
	rm test-ina219 *.o *.a