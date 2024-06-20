#include "my_delay.h"
#include "at32f403a_407_clock.h"
#include "lv_port_disp_template.h"

#include <stdlib.h>
// #include <unistd.h>

#include "lvgl.h"
#include "my_gui.h"

int main(void)
{
	system_clock_config();
	delay_init();
	tmr_clk_config();
	delay_ms(500);
	lv_init(); /* lvgl ϵͳ��ʼ�� */
	lv_port_disp_init(); /* lvgl ��ʾ�ӿڳ�ʼ��,���� lv_init()�ĺ��� */
	LV_LOG_USER("LVGL initialization completed!");

	/*Run the demo*/
	my_gui_init();

	while(1) {
		/* Periodically call the lv_task handler.
		 * It could be done in a timer interrupt or an OS task too.*/
		lv_task_handler();
		delay_ms(10);       /*Just to let the system breath*/
	}
	return 0;
}
