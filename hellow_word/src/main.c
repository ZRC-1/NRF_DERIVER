#include <zephyr/kernel.h>
#include <stdio.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#define BUTTON_NODE DT_ALIAS(user_key_1)
//#define BUTTON_NODE  DT_NODELABEL(button0)
#define LED_NODE DT_NODELABEL(led0)
//#define LED1_NODE DT_NODELABEL(led1)
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static struct gpio_dt_spec button_struct = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static struct gpio_dt_spec led_struct = GPIO_DT_SPEC_GET(LED_NODE, gpios);
static struct gpio_callback button_callback_struct;
// struct gpio_dt_spec led1_struct = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

uint8_t int_flag=0;
//botton0 IO中断回调函数
void gpio_callback(const struct device *port,struct gpio_callback *cb,gpio_port_pins_t pins)
{
        if(pins==BIT(button_struct.pin))
        {
                printk("Button pressed!\n");
                int_flag=1;
        }
}


int main(void)
{
        int ret;
        printk("Hello World! Running on %s\n", CONFIG_BOARD_TARGET);
        if(gpio_is_ready_dt(&button_struct)==false)
        {
                printk("button_init_flase");
        }
        if(gpio_is_ready_dt(&led_struct)==false)
        {
                printk("led_init_flase");
        }
        // if(gpio_is_ready_dt(&led1_struct)==false)
        // {
        //         printk("led1_init_flase");
        // }
        
        ret=gpio_pin_configure_dt(&led_struct, GPIO_OUTPUT|GPIO_ACTIVE_HIGH);        
        if(ret<0)
        {
                printk("gpio_pin_configure failed: ret=%d\n", ret);
        }
        ret=gpio_pin_configure_dt(&button_struct, GPIO_INPUT|GPIO_ACTIVE_HIGH);
        if(ret<0)
        {
                printk("gpio_pin_configure failed: ret=%d\n",ret);
        }
        // ret=gpio_pin_configure_dt(&led1_struct, GPIO_OUTPUT);
        // if(ret<0)
        // {
        //         printk("gpio_pin_configure failed: ret=%d\n",ret);
        // }
        ret=gpio_pin_interrupt_configure_dt(&button_struct, GPIO_INT_EDGE_TO_ACTIVE);
        if(ret<0)
        {
                printk("button_int_config_flase");
        }
        gpio_init_callback(&button_callback_struct, gpio_callback, BIT(button_struct.pin));
        ret=gpio_add_callback_dt(&button_struct,&button_callback_struct);
        if (ret<0)
        {
                printk("button_int_add_flase");
        }

        // gpio_pin_set_dt(&led1_struct,1);
        gpio_pin_set_dt(&led_struct,1);
        while (1)
        {
                // gpio_pin_get_dt() 返回逻辑电平（考虑ACTIVE_LOW）
                // gpio_pin_get_raw() 返回物理电平（不考虑ACTIVE_LOW）
                int logical_state = gpio_pin_get_dt(&button_struct);
                int physical_state = gpio_pin_get_raw(button_struct.port, button_struct.pin);
                LOG_INF("logical=%d, physical=%d\n", logical_state, physical_state);
                //printk("button_struct.pin=%d\n",button_struct.pin);
                k_sleep(K_MSEC(1000));
                if(int_flag==1)
                {
                        int_flag=0;
                        LOG_INF("int_flag=1\n");  
                }
        }
        return 0;       
}
