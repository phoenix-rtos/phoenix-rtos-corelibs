#
# Graph test Makefile
#

CC=bcc32
AS=tasm32
LD=tlink32
LIB=tlib

CFLAGS = -w-par -c -IC:\BORLAND\INCLUDE;..\kernel;. -D_BCC
ASFLAGS = /ml
LDFLAGS = -Tpe -ax -s -v

OBJS = chk.obj graph.obj soft.obj ct69000.obj savage4.obj geode.obj


test.exe: test.obj graph.lib
  $(LD) @&&|
  $(LDFLAGS) +
C:\BORLAND\LIB\c0x32.obj+
test.obj
$<,$*
graph.lib+
..\kernel\kernel.lib+
C:\BORLAND\LIB\dpmi32.lib+
C:\BORLAND\LIB\cw32.lib

|


graph.lib: $(OBJS)
  $(LIB) graph.lib @&&|
-+graph.obj &
-+chk.obj &
-+soft.obj &
-+ct69000.obj &
-+savage4.obj &
-+geode.obj


$<,$*

|


test.obj: test.cpp
  $(CC) $(CFLAGS) test.cpp


graph.obj: graph.cpp
  $(CC) $(CFLAGS) graph.cpp


chk.obj: chk.cpp
  $(CC) $(CFLAGS) chk.cpp


soft.obj: soft.cpp
  $(CC) $(CFLAGS) soft.cpp


ct69000.obj: ct69000.cpp
  $(CC) $(CFLAGS) ct69000.cpp


savage4.obj: savage4.cpp
  $(CC) $(CFLAGS) savage4.cpp


geode.obj: geode.cpp
  $(CC) $(CFLAGS) geode.cpp
