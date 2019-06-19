ffsocket: LDFLAGS += -lfftw3 -lasound

ffsocket: main.cpp
	$(CXX) -o $@ $^ $(LDFLAGS)
