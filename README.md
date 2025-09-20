# find_load_module-tool
本工具用于在无Linux内核源码的情况下，查找出内核函数load_module的位置，确保在无内核源码的情况下，顺利去除所有加载驱动的验证，此工具适用于所有Linux ARM64内核
# FindLoadModule: 无源码 Linux 内核函数定位器
本工具的目标是让开发人员在无源码环境下，能够顺利去除驱动加载的验证，从而让驱动在无源码的内核情况下正常运行
# 兼容性
FindLoadModule 可以广泛应用于所有 Linux ARM64 内核。无论使用何种版本的 ARM64 内核，都可以依赖 FindLoadModule 工具来定位 load_module 函数
# 使用方法
1.首先，将FindLoadModule编译出来 (这个源码已经修改，可以直接编译）
2.然后，将Android内核的boot.img文件解包，得到image镜像文件
3.如果镜像文件是压缩的（例如，image.gz），你需要进一步解压，得到实际的内核二进制文件
4.将这个内核二进制文件拖入此工具，你将直接得到load_module的位置
5.使用IDA跳转至该位置，浏览上下文。你将看到非常熟悉的load_module，进行适当的修改，即可去除所有驱动加载的验证
