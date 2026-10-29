# 사용법 (MSYS2 mingw64 환경):
#   mingw32-make          최적화(-O2) 빌드 -> main.exe
#   mingw32-make run      빌드 후 실행
#   mingw32-make clean    빌드 결과물 삭제
#
# 소스 파일은 폴더별 *.cpp를 자동으로 찾으므로 새 파일을 추가해도 이 파일을 고칠 필요가 없다.
# 헤더가 바뀌면 그 헤더를 쓰는 파일만 다시 컴파일한다(-MMD).

ifneq ($(wildcard C:/msys64/mingw64/bin/g++.exe),)
CXX := C:/msys64/mingw64/bin/g++.exe
else
CXX := g++
endif

TARGET   := main.exe

SRCS     := $(wildcard Main/*.cpp) \
            $(wildcard Math/*.cpp) \
            $(wildcard Physics/*.cpp) \
            $(wildcard Renderer/*.cpp) \
            $(wildcard Scenario/*.cpp)

OBJS     := $(SRCS:.cpp=.o)
DEPS     := $(OBJS:.o=.d)

# -O2를 빼면 프레임이 10배 가까이 느려진다.
CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -MMD -MP -IC:/msys64/mingw64/include
LDFLAGS  := -LC:/msys64/mingw64/lib
LDLIBS   := -lsfml-graphics -lsfml-window -lsfml-system -lOpenCL

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	-$(RM) $(OBJS) $(DEPS) $(TARGET)

-include $(DEPS)
