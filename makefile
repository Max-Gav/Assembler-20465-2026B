assembler: main.o assembler.o parser.o preprocessor.o files.o hashTable.o encoding.o
	gcc -ansi -Wall -pedantic -o assembler main.o assembler.o parser.o preprocessor.o files.o hashTable.o encoding.o

main.o: main.c main.h assembler.h files.h
	gcc -ansi -Wall -pedantic -c main.c

assembler.o: assembler.c assembler.h assembler_types.h encoding.h files.h hashTable.h parser.h preprocessor.h
	gcc -ansi -Wall -pedantic -c assembler.c

parser.o: parser.c parser.h assembler_types.h
	gcc -ansi -Wall -pedantic -c parser.c

preprocessor.o: preprocessor.c preprocessor.h assembler_types.h parser.h
	gcc -ansi -Wall -pedantic -c preprocessor.c

files.o: files.c files.h assembler_types.h
	gcc -ansi -Wall -pedantic -c files.c

hashTable.o: hashTable.c hashTable.h assembler_types.h
	gcc -ansi -Wall -pedantic -c hashTable.c

encoding.o: encoding.c encoding.h assembler_types.h
	gcc -ansi -Wall -pedantic -c encoding.c