# Algorithms 示例入口

[`alg_demo`](alg_demo/main.cpp) 是算法与数值处理的 host fixture，构建入口见
[`CMakeLists.txt`](alg_demo/CMakeLists.txt)。它覆盖 FFT、EWMA/Kalman/Biquad、mean/median、
YUV/HSV/gamma 转换以及 RLE/PackBits/Heatshrink/LZ4 压缩路径。

该 fixture 只验证当前接口和样本，不是产品算法库的完整能力声明。
