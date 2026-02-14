#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/i2c.h>
#include <strings.h>

#define BUTTON_NODE DT_ALIAS(user_key_1)
//#define BUTTON_NODE  DT_NODELABEL(button0)
#define LED_NODE DT_NODELABEL(led0)

//#define LED1_NODE DT_NODELABEL(led1)
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);
static struct gpio_dt_spec button_struct = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static struct gpio_dt_spec led_struct = GPIO_DT_SPEC_GET(LED_NODE, gpios);
static struct gpio_callback button_callback_struct;

// struct gpio_dt_spec led1_struct = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

uint8_t int_flag = 0;
uint8_t uart1_rx_buf[128] = {0};    // 接收缓冲区
uint8_t uart1_tx_buf[128] = {0};    // 发送缓冲区，独立存储
bool tx_busy = false;               // 发送忙标志

///配置i2c
#define I2C_NODE DT_ALIAS(user_i2c)
static const struct device *const i2c_dev = DEVICE_DT_GET(I2C_NODE);
static char last_byte;
// I2C Target 回调函数
static int sample_target_write_requested_cb(struct i2c_target_config *config)
{
	LOG_INF("I2C target write requested");
	return 0;
}

static int sample_target_write_received_cb(struct i2c_target_config *config, uint8_t val)
{
	LOG_INF("I2C target write received: 0x%02x", val);
	last_byte = val;
	return 0;
}

static int sample_target_read_requested_cb(struct i2c_target_config *config, uint8_t *val)
{
	LOG_INF("I2C target read requested");
	*val = last_byte;
	return 0;
}

static int sample_target_read_processed_cb(struct i2c_target_config *config, uint8_t *val)
{
	LOG_INF("I2C target read processed");
	*val = last_byte + 1;
	return 0;
}

static int sample_target_stop_cb(struct i2c_target_config *config)
{
	LOG_INF("I2C target stop");
	return 0;
}

static const struct i2c_target_callbacks sample_target_callbacks = {
	.write_requested = sample_target_write_requested_cb,
	.write_received = sample_target_write_received_cb,
	.read_requested = sample_target_read_requested_cb,
	.read_processed = sample_target_read_processed_cb,
	.stop = sample_target_stop_cb,
};

static struct i2c_target_config user_target_config = {
	.address = 0x12,
	.callbacks = &sample_target_callbacks,
};

//配置串口
#define UART1_NODE DT_ALIAS(uart1_use_protocol)
static const struct device *const uart1_dev = DEVICE_DT_GET(UART1_NODE);

void UART1_CALLBACK(const struct device *dev, struct uart_event *evt, void *user_data)
{
        switch (evt->type) {
        case UART_RX_RDY:
                // RX超时触发，复制数据到发送缓冲区并发送
                if (evt->data.rx.len > 0 && !tx_busy) 
                {
                        LOG_INF("RX timeout: %d bytes, sending back", evt->data.rx.len);
                        
                        // 复制数据到发送缓冲区
                        memcpy(uart1_tx_buf, &evt->data.rx.buf[evt->data.rx.offset], 
                               evt->data.rx.len);
                        
                        // 发送数据
                        tx_busy = true;
                        uart_tx(dev, uart1_tx_buf, evt->data.rx.len, SYS_FOREVER_MS);
                }
                break;

        case UART_RX_BUF_REQUEST:
                // memset(uart1_tx_buf, 0, sizeof(uart1_tx_buf));
                // uart_rx_enable(dev, uart1_rx_buf, sizeof(uart1_rx_buf), 500);
                break;

        case UART_RX_DISABLED:
                // 接收被禁用
                LOG_DBG("RX disabled");
                break;

        case UART_TX_DONE:
                // 发送完成，清空缓冲区并重启接收
                LOG_DBG("TX done: %d bytes", evt->data.tx.len);
                tx_busy = false;
                memset(uart1_rx_buf, 0, sizeof(uart1_rx_buf));
                memset(uart1_tx_buf, 0, sizeof(uart1_tx_buf));
                uart_rx_enable(dev, uart1_rx_buf, sizeof(uart1_rx_buf), 500);
                break;

        case UART_TX_ABORTED:
                // 发送中止
                LOG_WRN("TX aborted");
                break;

        default:
                break;
        }
}

void gpio_callback(const struct device *port,struct gpio_callback *cb,gpio_port_pins_t pins)
{
        if(pins==BIT(button_struct.pin))
        {
                LOG_DBG("Button pressed");
                int_flag=1;
        }
}


int main(void)
{
        int ret;
        LOG_INF("Hello World! Running on %s", CONFIG_BOARD_TARGET);
        if(gpio_is_ready_dt(&button_struct)==false)
        {
                LOG_ERR("button_init_failed");
        }
        if(gpio_is_ready_dt(&led_struct)==false)
        {
                LOG_ERR("led_init_failed");
        }
        // if(gpio_is_ready_dt(&led1_struct)==false)
        // {
        //         printk("led1_init_flase");
        // }
        
        ret=gpio_pin_configure_dt(&led_struct, GPIO_OUTPUT|GPIO_ACTIVE_HIGH);        
        if(ret<0)
        {
                LOG_ERR("gpio_pin_configure failed: ret=%d", ret);
        }
        ret=gpio_pin_configure_dt(&button_struct, GPIO_INPUT|GPIO_ACTIVE_HIGH);
        if(ret<0)
        {
                LOG_ERR("gpio_pin_configure failed: ret=%d", ret);
        }
        // ret=gpio_pin_configure_dt(&led1_struct, GPIO_OUTPUT);
        // if(ret<0)
        // {
        //         printk("gpio_pin_configure failed: ret=%d\n",ret);
        // }
        ret=gpio_pin_interrupt_configure_dt(&button_struct, GPIO_INT_EDGE_TO_ACTIVE);
        if(ret<0)
        {
                LOG_ERR("button_int_config_failed");
        }
        gpio_init_callback(&button_callback_struct, gpio_callback, BIT(button_struct.pin));
        ret=gpio_add_callback_dt(&button_struct,&button_callback_struct);
        if (ret<0)
        {
                LOG_ERR("button_int_add_failed");
        }
        //串口初始化
        if (!device_is_ready(uart1_dev)) {
                LOG_ERR("UART device not ready");
                return -1;
        }
        LOG_INF("UART1 device ready");
        ret = uart_callback_set(uart1_dev, UART1_CALLBACK, NULL);
        if (ret < 0) {
                LOG_ERR("uart_callback_set failed: %d", ret);
                return -1;
        }
        ret = uart_rx_enable(uart1_dev, uart1_rx_buf, sizeof(uart1_rx_buf), 500);
        if (ret < 0) {
                LOG_ERR("uart_rx_enable failed: %d", ret);
                return -1;
        }
        LOG_INF("UART1 initialized successfully");
        //I2C初始化
        if (!device_is_ready(i2c_dev)) 
        {                        
                LOG_ERR("I2C device not ready");
                return -1;
        }
        if (i2c_target_register(i2c_dev, &user_target_config) < 0) 
        {
		printk("Failed to register target\n");
		return -1;
	}
        

        uint8_t i2c_sendbuff[5]={0,1,2,3,4};
        // gpio_pin_set_dt(&led1_struct,1);
        gpio_pin_set_dt(&led_struct,1);
        while (1)
        {
                i2c_write(i2c_dev,i2c_sendbuff,5,user_target_config.address);

                k_sleep(K_MSEC(1000));
                if(int_flag==1)
                {
                        int_flag=0;
                }
        }
        return 0;       
}
