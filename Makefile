# 编译工具设置
CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2

# 源文件和头文件
SRCS = find_load_module.cpp analyze_kernel.cpp kernel_symbol_parser.cpp kernel_version_parser.cpp kallsyms_lookup_name.cpp kallsyms_lookup_name_4_6_0.cpp
HDRS = base_func.h analyze_kernel.h kernel_symbol_parser.h kernel_version_parser.h kallsyms_lookup_name.h kallsyms_lookup_name_4_6_0.h

# 目标文件名
TARGET = find_load_module

# 默认目标
all: $(TARGET)

# 生成目标文件
$(TARGET): $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

# 清理生成的文件
clean:
	rm -f $(TARGET) *.o

# Windows环境下的清理
clean_win:
	del $(TARGET).exe *.o

# 便于在Windows命令行中使用
win: 
	$(CXX) $(CXXFLAGS) -o $(TARGET).exe $(SRCS)