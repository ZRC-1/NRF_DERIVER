# UART 异步 API vs 轮询 API

## 🔴 错误原因

错误码 `-134` = `-ENOTSUP` (不支持的操作)

**原因**: 在使用 UART 异步 API 时调用了 `uart_configure()`，这个函数只适用于轮询/中断驱动模式。

## 📊 UART API 对比

### 1. 轮询模式 (Polling)

**特点**:
- 阻塞式读写
- 简单但效率低
- 适合简单应用

**初始化**:
```c
const struct uart_config cfg = {
    .baudrate = 115200,
    .parity = UART_CFG_PARITY_NONE,
    .stop_bits = UART_CFG_STOP_BITS_1,
    .data_bits = UART_CFG_DATA_BITS_8,
    .flow_ctrl = UART_CFG_FLOW_CTRL_NONE
};

uart_configure(uart_dev, &cfg);  // ✅ 轮询模式需要
```

**使用**:
```c
// 发送
uart_poll_out(uart_dev, 'A');

// 接收
char c;
uart_poll_in(uart_dev, &c);
```

---

### 2. 中断驱动模式 (Interrupt-driven)

**特点**:
- 非阻塞
- 使用 FIFO
- 中等复杂度

**配置**:
```properties
CONFIG_UART_INTERRUPT_DRIVEN=y
```

**初始化**:
```c
uart_configure(uart_dev, &cfg);  // ✅ 中断模式需要
uart_irq_callback_set(uart_dev, uart_isr);
uart_irq_rx_enable(uart_dev);
```

**使用**:
```c
void uart_isr(const struct device *dev, void *user_data) {
    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            uart_fifo_read(dev, &data, 1);
        }
    }
}
```

---

### 3. 异步模式 (Async) ⭐ 我们使用的

**特点**:
- 完全异步，基于事件
- 使用 DMA，效率最高
- 适合高速通信

**配置**:
```properties
CONFIG_UART_ASYNC_API=y
CONFIG_UART_1_ASYNC=y
```

**初始化**:
```c
// ❌ 不要调用 uart_configure()！
// 配置在设备树中完成

// ✅ 只需要设置回调和启动接收
uart_callback_set(uart_dev, uart_callback, NULL);
uart_rx_enable(uart_dev, rx_buf, sizeof(rx_buf), timeout);
```

**设备树配置**:
```dts
&uart1 {
    status = "okay";
    current-speed = <115200>;  // 波特率在这里配置
};

&uart1_default {
    group1 {
        psels = <NRF_PSEL(UART_TX, 0, 20)>,
                <NRF_PSEL(UART_RX, 0, 24)>;
    };
};
```

**使用**:
```c
void uart_callback(const struct device *dev, 
                   struct uart_event *evt, 
                   void *user_data) {
    switch (evt->type) {
    case UART_RX_RDY:
        // 处理接收到的数据
        break;
    case UART_TX_DONE:
        // 发送完成
        break;
    }
}

// 发送
uart_tx(uart_dev, tx_buf, len, timeout);
```

---

## 🎯 关键区别

| 特性 | 轮询 | 中断驱动 | 异步 (DMA) |
|------|------|----------|------------|
| `uart_configure()` | ✅ 需要 | ✅ 需要 | ❌ 不需要 |
| 配置位置 | 代码 | 代码 | 设备树 |
| CPU 占用 | 高 | 中 | 低 |
| 效率 | 低 | 中 | 高 |
| 复杂度 | 简单 | 中等 | 复杂 |
| DMA | ❌ | ❌ | ✅ |

---

## ✅ 正确的异步 UART 初始化

### 设备树配置 (nrf52840dk_nrf52840.overlay)

```dts
/{
    aliases {
        uart1-use-protocol = &uart1;
    };
};

&uart1 {
    status = "okay";
    current-speed = <115200>;  // 波特率
    // 其他参数使用默认值：
    // - 8 数据位
    // - 无奇偶校验
    // - 1 停止位
    // - 无流控
};

&uart1_default {
    group1 {
        psels = <NRF_PSEL(UART_TX, 0, 20)>,
                <NRF_PSEL(UART_RX, 0, 24)>;
    };
};
```

### Kconfig 配置 (prj.conf)

```properties
# 启用 UART 异步 API
CONFIG_UART_ASYNC_API=y
CONFIG_UART_1_ASYNC=y
CONFIG_UART_1_INTERRUPT_DRIVEN=y
```

### C 代码 (main.c)

```c
#define UART1_NODE DT_ALIAS(uart1_use_protocol)
static const struct device *const uart1_dev = DEVICE_DT_GET(UART1_NODE);

uint8_t uart1_rx_buf[50] = {0};
uint8_t *uart1_rx_ptr = uart1_rx_buf;

void UART1_CALLBACK(const struct device *dev, 
                    struct uart_event *evt, 
                    void *user_data) {
    switch (evt->type) {
    case UART_RX_RDY:
        if ((uart1_rx_ptr - uart1_rx_buf) < sizeof(uart1_rx_buf)) {
            *uart1_rx_ptr = *(evt->data.rx.buf + evt->data.rx.offset);
            uart1_rx_ptr++;
        }
        break;
    case UART_RX_DISABLED:
        if (uart1_rx_ptr > uart1_rx_buf) {
            uart_tx(uart1_dev, uart1_rx_buf, 
                   (uart1_rx_ptr - uart1_rx_buf), SYS_FOREVER_US);
        }
        uart1_rx_ptr = uart1_rx_buf;
        uart_rx_enable(uart1_dev, uart1_rx_buf, 
                      sizeof(uart1_rx_buf), SYS_FOREVER_US);
        break;
    case UART_TX_DONE:
        printk("TX done\n");
        break;
    default:
        break;
    }
}

int main(void) {
    int ret;
    
    // 1. 检查设备就绪
    if (!device_is_ready(uart1_dev)) {
        printk("UART device not ready\n");
        return -1;
    }
    
    // 2. 设置回调（不需要 uart_configure！）
    ret = uart_callback_set(uart1_dev, UART1_CALLBACK, NULL);
    if (ret < 0) {
        printk("uart_callback_set failed: %d\n", ret);
        return -1;
    }
    
    // 3. 启动接收
    ret = uart_rx_enable(uart1_dev, uart1_rx_buf, 
                        sizeof(uart1_rx_buf), SYS_FOREVER_US);
    if (ret < 0) {
        printk("uart_rx_enable failed: %d\n", ret);
        return -1;
    }
    
    printk("UART1 initialized successfully\n");
    
    while (1) {
        k_sleep(K_MSEC(1000));
    }
    
    return 0;
}
```

---

## ❌ 常见错误

### 错误1：在异步模式调用 uart_configure()

```c
// ❌ 错误
const struct uart_config cfg = {...};
uart_configure(uart1_dev, &cfg);  // 返回 -ENOTSUP (-134)
```

**原因**: 异步 UART 不支持运行时配置，必须在设备树中配置。

### 错误2：忘记启用异步 API

```properties
# ❌ 缺少配置
# CONFIG_UART_ASYNC_API=y  # 忘记启用
```

**结果**: `uart_rx_enable()` 等函数不可用。

### 错误3：设备树中没有配置波特率

```dts
# ❌ 缺少 current-speed
&uart1 {
    status = "okay";
    # current-speed = <115200>;  # 忘记配置
};
```

**结果**: 使用默认波特率（可能不是你想要的）。

---

## 🔧 调试技巧

### 检查 UART 是否支持异步模式

```c
if (!device_is_ready(uart1_dev)) {
    printk("Device not ready\n");
    return -1;
}

// 尝试设置回调
int ret = uart_callback_set(uart1_dev, callback, NULL);
if (ret == -ENOTSUP) {
    printk("Async API not supported\n");
} else if (ret < 0) {
    printk("Callback set failed: %d\n", ret);
}
```

### 查看生成的设备树

```bash
# 查看最终的设备树配置
cat build/hellow_word/zephyr/zephyr.dts | grep -A 20 "uart@40028000"
```

### 检查 Kconfig

```bash
# 查看 UART 配置
cat build/hellow_word/zephyr/.config | grep UART
```

---

## 📚 参考

- [Zephyr UART API 文档](https://docs.zephyrproject.org/latest/hardware/peripherals/uart.html)
- [nRF UART 驱动](https://docs.zephyrproject.org/latest/build/dts/api/bindings/serial/nordic,nrf-uarte.html)

---

## 🎓 总结

**异步 UART 的关键点**:

1. ❌ **不要**调用 `uart_configure()`
2. ✅ 在设备树中配置波特率和引脚
3. ✅ 在 prj.conf 中启用异步 API
4. ✅ 只需调用 `uart_callback_set()` 和 `uart_rx_enable()`
5. ✅ 在回调中处理所有 UART 事件

**记住**: 异步模式 = 设备树配置 + 事件驱动，不需要 `uart_configure()`！
