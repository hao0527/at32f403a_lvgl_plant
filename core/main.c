#include "my_delay.h"
#include "at32f403a_407_clock.h"
#include "lv_port_disp_template.h"

int main(void)
{
	system_clock_config();
	delay_init();
	delay_ms(500);
	lv_init(); /* lvgl ϵͳ��ʼ�� */
	lv_port_disp_init(); /* lvgl ��ʾ�ӿڳ�ʼ��,���� lv_init()�ĺ��� */
	lv_obj_t *label = lv_label_create(lv_scr_act());
	lv_label_set_text(label, "Hello LVGL  :- (");
	lv_obj_center(label);
	while(1)
	{
		lv_timer_handler();
	}
}
