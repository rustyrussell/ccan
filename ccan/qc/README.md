# qc - A C port of the QuickCheck unit test framework

## HOMEPAGE

[http://www.yellosoft.us/quickcheck](http://www.yellosoft.us/quickcheck)

## REQUIREMENTS

 - [BoehmGC](http://www.hpl.hp.com/personal/Hans_Boehm/gc/)

## EXAMPLE

	$ make
	gcc -o example example.c qc.c qc.h -lgc -Wall
	$ ./example 
	+++ OK, passed 100 tests.
	*** Failed!
	453045430
	*** Failed!
	1505013395
	1697735399
	223633304
	*** Failed!
	'4'
	4
	*** Failed!
	ff<GC"z<z #9_zN	1.>w`Uhl~M~; >FTWjt{qQ+0Iek~