SRCS = $(wildcard coroutines/*.cpp)
SRCS += $(wildcard coroutines/api/*_linux.cpp)
SRCS += vc2015/sample00.cpp

$(info SRCS are $(SRCS))

OBJS = $(foreach f,$(SRCS),objs/$(notdir $(basename $(f))).o)

CPPFLAGS = -std=c++11
LNKFLAGS = -lstdc++

objs/%.o : coroutines/%.cpp
	@echo C++ $<...
	mkdir -p objs
	$(CC) $(CPPFLAGS) -c $< -o $@

objs/%.o : coroutines/api/%.cpp
	@echo C++ $<...
	mkdir -p objs
	$(CC) $(CPPFLAGS) -c $< -o $@

objs/%.o : vc2015/%.cpp
	@echo C++ $<...
	@mkdir -p objs
	@$(CC) -c $< -o $@ $(CPPFLAGS)

demo : $(OBJS)
	@echo Linking... $(OBJS)
	@$(CC) $(LNKFLAGS) -o $@ $(OBJS)

clean :
	rm -rf objs