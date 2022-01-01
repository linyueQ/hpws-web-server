CFLAGS=-std=c++14 -O2 -Wall -g
src=$(wildcard *.cpp)
obj=$(patsubst %.cpp, %.o, $(src))
target=test
$(target):$(obj)
	$(CXX) $(CFLAGS) $(obj) -o $(target)

%.o:%.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

.PHONY:clean
clean:
	rm $(obj) -f