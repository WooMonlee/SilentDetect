CXX = x86_64-w64-mingw32-g++
WINDRES = windres

CXXFLAGS = -static -O2 -mwindows -municode -std=c++17 -DUNICODE -D_UNICODE
LDFLAGS = -lcomctl32 -lshell32 -lole32 -luuid -lgdi32 -lcomdlg32 -luser32 -lshlwapi

TARGET = SilentParamQuery.exe
SRCS = main.cpp scanner.cpp batgen.cpp batchrun.cpp
RES_RC = resource.rc
RES_O = resource.o
TOOLS_RC = tools_resource.rc
TOOLS_O = tools_resource.o

.PHONY: all release debug clean

# 默认：发布版（无调试日志）
all: release

# 发布版：日志函数编译为空操作
release:
	$(WINDRES) $(RES_RC) -o $(RES_O) --input-format=rc --output-format=coff
	$(WINDRES) $(TOOLS_RC) -o $(TOOLS_O) --input-format=rc --output-format=coff
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(RES_O) $(TOOLS_O) $(LDFLAGS)

# 测试版：启用DEBUG_LOG，输出debug_log.txt记录运行详情
debug:
	$(WINDRES) $(RES_RC) -o $(RES_O) --input-format=rc --output-format=coff
	$(WINDRES) $(TOOLS_RC) -o $(TOOLS_O) --input-format=rc --output-format=coff
	$(CXX) $(CXXFLAGS) -DDEBUG_LOG -o $(TARGET) $(SRCS) $(RES_O) $(TOOLS_O) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(RES_O) $(TOOLS_O)
