# UART轮询模式发送问题诊断

## 🔴 问题
`uart_poll_out(uart1_dev, 'a');` 发送后接收不到数据

## 🔍 检查清单

### ✅ 1. 设备树配置（已检查）
```dts
&uart1 {
    status = "okay";
    current-speed = <115200>;
};

&uart1_default {
    group1 {
        psels = <NRF_PSEL(UART_TX, 0, 22)>,  // P0.22 = TX
                <NRF_PSEL(UART_RX, 0, 24)>;  // P0.24 = RX
    };
};
```
**状态**: ✅ 配置正确

### ⚠️ 2. 模式冲突问题

**问题**: 你同时启用了异步模式和轮询模式

```properties
# prj.conf
CONFIG_UART_ASYNC_API=y      # 异步模式
CONFIG_UART_1_ASYNC=y         # UART1异步（但配置失败）
```

**uart_poll_out()** 是轮询模式API，与异步模式可能冲突。

### 🔧 解决方案

#### 方案1：纯轮询模式（简单）

**修改 prj.conf**:
```properties
CONFIG_LOG=y
CONFIG_GPIO=y
CONFIG_UART=y

# 不启用异步模式
# CONFIG_UART_ASYNC_API=n
# CONFIG_UART_1_ASYNC=n
```

**测试代码**:
```c
// 轮询发送
uart_poll_out(uart1_dev, 'H');
uart_poll_out(uart1_dev, 'i');
uart_poll_out(uart1_dev, '\r');
uart_poll_out(uart1_dev, '\n');

// 轮询接收
unsigned char c;
while (1) {
    if (uart_poll_in(uart1_dev, &c) == 0) {
        printk("Received: %c\n", c);
        // 回显
        uart_poll_out(uart1_dev, c);
    }
    k_msleep(10);
}
```

#### 方案2：纯异步模式（推荐）

**不要混用** `uart_poll_out()` 和异步API。

**只使用异步API**:
```c
// 异步发送
const char *msg = "Hello\r\n";
uart_tx(uart1_dev, msg, strlen(msg), 1000);
```

### 🔍 3. 硬件连接检查

**nRF52840DK UART1引脚**:
- TX: P0.22
- RX: P0.24

**检查**:
1. 确认串口工具连接到正确的COM口
2. 确认TX/RX没有接反
3. 确认波特率115200
4. 如果使用USB转串口，确认驱动正常

### 🔍 4. 引脚冲突检查

检查P0.22和P0.24是否被其他外设占用：

```bash
# 在生成的设备树中搜索
cat build/hellow_word/zephyr/zephyr.dts | grep "0x16\|0x18"
```

**当前状态**: 未发现冲突

### 🔍 5. UART设备就绪检查

**添加检查代码**:
```c
if (!device_is_ready(uart1_dev)) {
    printk("UART1 device not ready!\n");
    return -1;
}
printk("UART1 device ready\n");

// 测试轮询发送
printk("Sending 'a'...\n");
uart_poll_out(uart1_dev, 'a');
printk("Sent\n");
```

### 🔍 6. 日志后端冲突

**检查是否UART被日志占用**:

```properties
# prj.conf中确保
# CONFIG_LOG_BACKEND_UART=n  # 不要让日志占用UART
CONFIG_LOG_BACKEND_RTT=y     # 使用RTT输出日志
```

## 💡 推荐的完整配置

### prj.conf
```properties
CONFIG_LOG=y
CONFIG_GPIO=y

# UART异步模式
CONFIG_UART_ASYNC_API=y

# 日志使用RTT，不占用UART
CONFIG_USE_SEGGER_RTT=y
CONFIG_LOG_BACKEND_RTT=y
# CONFIG_LOG_BACKEND_UART=n

# 禁用NFC
CONFIG_NFCT=n
```

### 设备树 (nrf52840dk_nrf52840.overlay)
```dts
/{
    aliases {
        uart1-use-protocol = &uart1;
    };
};

&uart1 {
    status = "okay";
    current-speed = <115200>;
};

&uart1_default {
    group1 {
        psels = <NRF_PSEL(UART_TX, 0, 22)>,
                <NRF_PSEL(UART_RX, 0, 24)>;
    };
};
```

### 代码
```c
// 只使用异步API，不要混用uart_poll_out
void UART1_CALLBACK(const struct device *dev, struct uart_event *evt, void *user_data)
{
    switch (evt->type) {
    case UART_RX_RDY:
        // 接收数据
        *uart1_rx_ptr = *(evt->data.rx.buf + evt->data.rx.offset);
        uart1_rx_ptr++;
        break;
        
    case UART_RX_DISABLED:
        // 回显
        int len = uart1_rx_ptr - uart1_rx_buf;
        if (len > 0) {
            uart_tx(uart1_dev, uart1_rx_buf, len, 1000);
        }
        uart1_rx_ptr = uart1_rx_buf;
        uart_rx_enable(uart1_dev, uart1_rx, sizeof(uart1_rx), 1000);
        break;
    }
}
```

## 🧪 测试步骤

1. **清理并重新构建**:
```bash
west build -b nrf52840dk_nrf52840 -p
west flash
```

2. **打开串口工具**:
   - 波特率: 115200
   - 数据位: 8
   - 停止位: 1
   - 校验: 无
   - 流控: 无

3. **发送测试数据**:
   - 发送: `Hello`
   - 应该收到回显: `Hello`

4. **查看日志**:
   - 使用RTT Viewer或J-Link RTT查看调试日志

## 🎯 最可能的原因

1. **模式冲突**: 异步模式和轮询模式混用
2. **设备未就绪**: UART设备初始化失败
3. **硬件连接**: TX/RX接反或未连接
4. **日志占用**: UART被日志后端占用

## ✅ 快速修复

**如果只是想测试轮询发送，最简单的方法**:

```c
int main(void) {
    // 不初始化异步模式
    // 直接使用轮询
    
    if (!device_is_ready(uart1_dev)) {
        printk("UART not ready\n");
        return -1;
    }
    
    while (1) {
        uart_poll_out(uart1_dev, 'A');
        k_msleep(1000);
        
        unsigned char c;
        if (uart_poll_in(uart1_dev, &c) == 0) {
            printk("RX: %c\n", c);
            uart_poll_out(uart1_dev, c);  // 回显
        }
    }
}
```

这样就不会有模式冲突问题。
