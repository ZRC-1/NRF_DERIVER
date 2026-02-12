# UART Usage Fault 修复说明

## 🔴 原始错误

```
<err> os: ***** USAGE FAULT *****
<err> os:   Attempt to execute undefined instruction
<err> os: Faulting instruction address (r15/pc): 0x00023e0e
```

## 🐛 问题根源

### 问题1：空指针赋值（最严重）

**位置**: `UART1_CALLBACK` 函数

```c
// ❌ 错误代码
case UART_TX_ABORTED:
    uart_tx(uart1_dev, uart1_rx_buf, (uart1_rx_ptr - uart1_rx_buf), 5);
    uart1_rx_ptr = 0;  // ← 将指针设置为NULL！
    break;
```

**后果**:
- 下次接收数据时，`*uart1_rx_ptr = ...` 会尝试写入地址0
- 导致内存访问违规
- 触发 USAGE FAULT

### 问题2：UART事件处理不完整

原代码只处理了 `UART_RX_RDY` 和 `UART_TX_ABORTED`，缺少：
- `UART_RX_DISABLED` - 接收完成事件
- `UART_TX_DONE` - 发送完成事件
- 缓冲区溢出检查

### 问题3：设备树配置不规范

```dts
// ❌ 错误：TX和RX分在两个group
&uart1_default {
    group2 {
        psels = <NRF_PSEL(UART_TX, 0, 20)>;
    };
    group1 {
        psels = <NRF_PSEL(UART_RX, 0, 24)>;
    };
};
```

### 问题4：缺少UART异步API配置

`prj.conf` 中没有启用UART异步API。

## ✅ 修复方案

### 修复1：正确的指针重置

```c
// ✅ 正确代码
case UART_RX_DISABLED:
    // 接收完成，发送数据
    if (uart1_rx_ptr > uart1_rx_buf) {
        uart_tx(uart1_dev, uart1_rx_buf, (uart1_rx_ptr - uart1_rx_buf), SYS_FOREVER_US);
    }
    // 重置指针到缓冲区开始（不是NULL！）
    uart1_rx_ptr = uart1_rx_buf;  // ← 正确的重置方式
    // 重新启动接收
    uart_rx_enable(uart1_dev, uart1_rx_buf, sizeof(uart1_rx_buf), SYS_FOREVER_US);
    break;
```

### 修复2：完整的事件处理

```c
void UART1_CALLBACK(const struct device *dev, struct uart_event *evt, void *user_data)
{
    switch (evt->type) {
    case UART_RX_RDY:
        // 添加缓冲区溢出检查
        if ((uart1_rx_ptr - uart1_rx_buf) < sizeof(uart1_rx_buf)) {
            *uart1_rx_ptr = *(evt->data.rx.buf + evt->data.rx.offset);
            uart1_rx_ptr++;
        }
        break;
        
    case UART_RX_DISABLED:
        // 接收完成，处理数据
        if (uart1_rx_ptr > uart1_rx_buf) {
            uart_tx(uart1_dev, uart1_rx_buf, (uart1_rx_ptr - uart1_rx_buf), SYS_FOREVER_US);
        }
        uart1_rx_ptr = uart1_rx_buf;
        uart_rx_enable(uart1_dev, uart1_rx_buf, sizeof(uart1_rx_buf), SYS_FOREVER_US);
        break;
        
    case UART_TX_DONE:
        printk("TX done\n");
        break;
        
    case UART_TX_ABORTED:
        printk("TX aborted\n");
        break;
        
    default:
        break;
    }
}
```

### 修复3：规范的设备树配置

```dts
&uart1 {
    status = "okay";
    current-speed = <115200>;
    pinctrl-0 = <&uart1_default>;
    pinctrl-names = "default";
};

&uart1_default {
    group1 {
        psels = <NRF_PSEL(UART_TX, 0, 20)>,
                <NRF_PSEL(UART_RX, 0, 24)>;
    };
};
```

### 修复4：启用UART异步API

在 `prj.conf` 中添加：

```properties
CONFIG_UART_ASYNC_API=y
CONFIG_UART_1_ASYNC=y
CONFIG_UART_1_INTERRUPT_DRIVEN=y
```

### 修复5：添加设备就绪检查

```c
// 串口初始化
if (!device_is_ready(uart1_dev)) {
    printk("UART device not ready\n");
    return -1;
}

ret = uart_configure(uart1_dev, &uart1_cfg);
if (ret < 0) {
    printk("uart_config_failed: %d\n", ret);
    return -1;
}

ret = uart_callback_set(uart1_dev, UART1_CALLBACK, NULL);
if (ret < 0) {
    printk("uart_callback_set failed: %d\n", ret);
    return -1;
}

ret = uart_rx_enable(uart1_dev, uart1_rx_buf, sizeof(uart1_rx_buf), SYS_FOREVER_US);
if (ret < 0) {
    printk("uart_rx_enable failed: %d\n", ret);
    return -1;
}
```

## 🎯 UART异步API工作流程

```
1. 初始化
   uart_configure() → uart_callback_set() → uart_rx_enable()

2. 接收数据
   UART_RX_RDY → 数据写入缓冲区
   UART_RX_DISABLED → 接收完成

3. 发送数据
   uart_tx() → UART_TX_DONE

4. 重新启动接收
   uart_rx_enable()
```

## 📊 关键概念

### 指针管理

```c
// 初始化
uint8_t uart1_rx_buf[50] = {0};
uint8_t *uart1_rx_ptr = uart1_rx_buf;  // 指向缓冲区开始

// 接收数据
*uart1_rx_ptr = data;
uart1_rx_ptr++;  // 移动指针

// 重置（正确方式）
uart1_rx_ptr = uart1_rx_buf;  // ✅ 指向缓冲区开始

// 重置（错误方式）
uart1_rx_ptr = 0;  // ❌ NULL指针！下次写入会崩溃
```

### UART事件类型

| 事件 | 说明 | 何时触发 |
|------|------|----------|
| UART_RX_RDY | 接收到数据 | 每接收到字节 |
| UART_RX_DISABLED | 接收禁用 | 超时或缓冲区满 |
| UART_TX_DONE | 发送完成 | 数据发送完毕 |
| UART_TX_ABORTED | 发送中止 | 发送被取消 |
| UART_RX_BUF_REQUEST | 缓冲区请求 | 需要新缓冲区 |
| UART_RX_BUF_RELEASED | 缓冲区释放 | 缓冲区可重用 |

## 🔍 调试技巧

### 1. 添加调试输出

```c
case UART_RX_RDY:
    printk("RX: offset=%d, len=%d\n", 
           evt->data.rx.offset, 
           evt->data.rx.len);
    break;
```

### 2. 检查指针有效性

```c
if (uart1_rx_ptr == NULL || uart1_rx_ptr < uart1_rx_buf) {
    printk("ERROR: Invalid pointer!\n");
    uart1_rx_ptr = uart1_rx_buf;
}
```

### 3. 监控缓冲区使用

```c
size_t used = uart1_rx_ptr - uart1_rx_buf;
printk("Buffer used: %d/%d\n", used, sizeof(uart1_rx_buf));
```

## ✅ 验证步骤

1. 重新构建项目
   ```bash
   west build -b nrf52840dk_nrf52840 -p
   ```

2. 烧录
   ```bash
   west flash
   ```

3. 查看日志
   ```bash
   # 应该看到
   Hello World! Running on nrf52840dk_nrf52840
   UART1 initialized successfully
   ```

4. 测试UART
   - 发送数据到UART1
   - 应该收到回显
   - 不应该有USAGE FAULT

## 📌 注意事项

1. **永远不要将指针设置为0或NULL**，除非你确实想要NULL指针
2. **总是检查缓冲区边界**，防止溢出
3. **处理所有相关的UART事件**，不要遗漏
4. **在使用设备前检查就绪状态**
5. **检查所有API调用的返回值**

## 🎓 总结

这个USAGE FAULT是由于将接收缓冲区指针错误地设置为NULL导致的。修复的关键是：
- 正确重置指针到缓冲区开始（不是NULL）
- 完整处理UART异步事件
- 添加边界检查和错误处理
- 正确配置设备树和Kconfig
