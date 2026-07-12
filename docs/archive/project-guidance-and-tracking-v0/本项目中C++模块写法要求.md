```c++
module;// 以模块声明开头
#include <cstdint> // 头文件导入区
export module audio.wav;    // 
// 声明模块后，如需导入其它模块，需连续，不能中途声明其它内容或引入头文件
import m1;  // 不需要导出的模块
import export m2;   // 
// 结束模块引入区

// 开始正文

static constexpr auto kVar = 10;

// 可单个导出
export namespace audio.wav {
    auto var1 = kVar;
}

// 可大括号批量导出
export {
    int var2; 
    int var3; 
    int var4; 
}

export "C" bool init() { return true; }

```

如需拆分多模块：

主模块：

```c++
export module daplink.usb_minimal;
export import :ep0;
```

子模块：

```c++
export module daplink.usb_minimal:ep0;
```
