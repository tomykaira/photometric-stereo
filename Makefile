OPENCV_OPTS=-lopencv_gpu -lopencv_contrib -lopencv_legacy -lopencv_objdetect -lopencv_calib3d -lopencv_features2d -lopencv_video -lopencv_highgui -lopencv_ml -lopencv_imgproc -lopencv_flann -lopencv_core -lopencv_gpu -lopencv_contrib -lopencv_legacy -lopencv_objdetect -lopencv_calib3d -lopencv_features2d -lopencv_video -lopencv_highgui -lopencv_ml -lopencv_imgproc -lopencv_flann -lopencv_core -I/usr/include/opencv
OPENGL_OPTS=-lglut -lGLU -lGL
LDFLAGS=$(OPENCV_OPTS) $(OPENGL_OPTS) -ldbus-1 -lboost_thread-mt

CPPFLAGS=-g -std=c++11 -Wall -Werror -I/usr/include/dbus-1.0/ -I/usr/lib/i386-linux-gnu/dbus-1.0/include/ -I.

PROG=solver
SRC=solver.cpp
OBJS=$(patsubst %.cpp,%.o,$(SRC))
DEPENDS=$(patsubst %.cpp,%.d,$(SRC))

$(PROG): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

.c.o:
	$(CXX) $(CPPFLAGS) -c $<

.PHONY : clean depend
clean:
	-$(RM) $(PROG) $(OBJS) $(DEPENDS)

%.d: %.cpp
	@set -e; $(CC) -MM $(CPPFLAGS) $< \
		| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@; \
		[ -s $@ ] || rm -f $@

-include $(DEPENDS)
