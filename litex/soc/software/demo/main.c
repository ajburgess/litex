// This file is Copyright (c) 2020 Florent Kermarrec <florent@enjoy-digital.fr>
// License: BSD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <irq.h>
#include <libbase/uart.h>
#include <libbase/console.h>
#include <generated/csr.h>

/*-----------------------------------------------------------------------*/
/* Uart                                                                  */
/*-----------------------------------------------------------------------*/

static char *readstr(void)
{
	char c[2];
	static char s[64];
	static int ptr = 0;

	if (readchar_nonblock())
	{
		c[0] = getchar();
		c[1] = 0;
		switch (c[0])
		{
		case 0x7f:
		case 0x08:
			if (ptr > 0)
			{
				ptr--;
				fputs("\x08 \x08", stdout);
			}
			break;
		case 0x07:
			break;
		case '\r':
		case '\n':
			s[ptr] = 0x00;
			fputs("\n", stdout);
			ptr = 0;
			return s;
		default:
			if (ptr >= (sizeof(s) - 1))
				break;
			fputs(c, stdout);
			s[ptr] = c[0];
			ptr++;
			break;
		}
	}

	return NULL;
}

static char *get_token(char **str)
{
	char *c, *d;

	c = (char *)strchr(*str, ' ');
	if (c == NULL)
	{
		d = *str;
		*str = *str + strlen(*str);
		return d;
	}
	*c = 0;
	d = *str;
	*str = c + 1;
	return d;
}

static void prompt(void)
{
	printf("\e[92;1mlitex-demo-app\e[0m> ");
}

/*-----------------------------------------------------------------------*/
/* Help                                                                  */
/*-----------------------------------------------------------------------*/

static void help(void)
{
	puts("\nLiteX minimal demo app built "__DATE__
		 " "__TIME__
		 "\n");
	puts("Available commands:");
	puts("help               - Show this command");
	puts("reboot             - Reboot CPU");
#ifdef CSR_LEDS_BASE
	puts("led                - Led demo");
#endif
#ifdef CSR_SOUND_GENERATOR_BASE
	puts("sound              - Sound demo");
#endif
#ifdef CSR_TIMER0_BASE
	puts("timer              - Timer demo");
#endif
	puts("donut              - Spinning Donut demo");
	puts("helloc             - Hello C");
#ifdef WITH_CXX
	puts("hellocpp           - Hello C++");
#endif
}

/*-----------------------------------------------------------------------*/
/* Commands                                                              */
/*-----------------------------------------------------------------------*/

static void reboot_cmd(void)
{
	ctrl_reset_write(1);
}

#ifdef CSR_TIMER0_BASE

static void timer_cmd_interrupt_handler(void)
{
	printf("Inside my interrupt handler!\n");
	timer1_ev_pending_write(1);
}

static void timer_cmd(void)
{
	printf("Timer demo...\n");

	// Disable the timer while we make changes.
	timer1_en_write(0);

	// Register our interrupt handler.
	irq_attach(TIMER1_INTERRUPT, timer_cmd_interrupt_handler);

	// Enable the timer in the CPU interrupt mask.
	unsigned int mask = irq_getmask();
	mask |= 1 << TIMER1_INTERRUPT;
	irq_setmask(mask);

	// Enable the timer's event-handling logic.
	timer1_ev_enable_write(1);

	// Make the timer generate a tick every 100ms (frequency = 10 Hz)
	timer1_load_write(0);
	timer1_reload_write(CONFIG_CLOCK_FREQUENCY / 10);

	// Re-enable the timer now we have finished making changes.
	timer1_en_write(1);

	int i, value;
	for (i = 0; i < 40; i++)
	{
		timer1_update_value_write(1);
		value = timer1_value_read();
		printf("Value: %d\n", value);
		busy_wait(100);
	}

	// Remove the timer from the CPU interrupt mask.
	mask = irq_getmask();
	mask &= ~(1 << TIMER1_INTERRUPT);
	irq_setmask(mask);
}

#endif

#ifdef CSR_SOUND_GENERATOR_BASE

static uint32_t get_midi_note_period(int note_number)
{
	uint32_t *addr = (uint32_t *)(CSR_SOUND_GENERATOR_MIDI_LOOKUP_BASE + (note_number << 2));
	uint32_t period = *addr;
	return period;
}

static void sound_cmd(void)
{
	int i;
	printf("Sound demo...\n");

	printf("Pure tone...\n");
	for (i = 0; i < 4; i++)
	{
		int period = get_midi_note_period(57);
		int amplitude = (i + 1) * 4 - 1;
		sound_generator_ch1_period_write(period);
		sound_generator_ch1_amplitude_write(amplitude);
		busy_wait(200);
		sound_generator_ch1_amplitude_write(0);
		busy_wait(200);
	}

	printf("Noise (low)...\n");
	for (i = 0; i < 4; i++)
	{
		sound_generator_ch4_period_write(512);
		sound_generator_ch4_amplitude_write(15);
		busy_wait(50);
		sound_generator_ch4_amplitude_write(0);
		busy_wait(350);
	}

	printf("Noise (high)...\n");
	for (i = 0; i < 4; i++)
	{
		sound_generator_ch4_period_write(128);
		sound_generator_ch4_amplitude_write(15);
		busy_wait(50);
		sound_generator_ch4_amplitude_write(0);
		busy_wait(350);
	}
}
#endif

#ifdef CSR_LEDS_BASE
static void led_cmd(void)
{
	int i;
	printf("Led demo...\n");

	printf("Counter mode...\n");
	for (i = 0; i < 32; i++)
	{
		leds_out_write(i);
		busy_wait(100);
	}

	printf("Shift mode...\n");
	for (i = 0; i < 4; i++)
	{
		leds_out_write(1 << i);
		busy_wait(200);
	}
	for (i = 0; i < 4; i++)
	{
		leds_out_write(1 << (3 - i));
		busy_wait(200);
	}

	printf("Dance mode...\n");
	for (i = 0; i < 4; i++)
	{
		leds_out_write(0x55);
		busy_wait(200);
		leds_out_write(0xaa);
		busy_wait(200);
	}
}
#endif

extern void donut(void);

static void donut_cmd(void)
{
	printf("Donut demo...\n");
	donut();
}

extern void helloc(void);

static void helloc_cmd(void)
{
	printf("Hello C demo...\n");
	helloc();
}

#ifdef WITH_CXX
extern void hellocpp(void);

static void hellocpp_cmd(void)
{
	printf("Hello C++ demo...\n");
	hellocpp();
}
#endif

/*-----------------------------------------------------------------------*/
/* Console service / Main                                                */
/*-----------------------------------------------------------------------*/

static void console_service(void)
{
	char *str;
	char *token;

	str = readstr();
	if (str == NULL)
		return;
	token = get_token(&str);
	if (strcmp(token, "help") == 0)
		help();
	else if (strcmp(token, "reboot") == 0)
		reboot_cmd();
#ifdef CSR_LEDS_BASE
	else if (strcmp(token, "led") == 0)
		led_cmd();
#endif
#ifdef CSR_SOUND_GENERATOR_BASE
	else if (strcmp(token, "sound") == 0)
		sound_cmd();
#endif
#ifdef CSR_TIMER0_BASE
	else if (strcmp(token, "timer") == 0)
	{
		printf("BLAH");
		timer_cmd();
	}
#endif
	else if (strcmp(token, "donut") == 0)
		donut_cmd();
	else if (strcmp(token, "helloc") == 0)
		helloc_cmd();
#ifdef WITH_CXX
	else if (strcmp(token, "hellocpp") == 0)
		hellocpp_cmd();
#endif
	prompt();
}

int main(void)
{
#ifdef CONFIG_CPU_HAS_INTERRUPT
	irq_setmask(0);
	irq_setie(1);
#endif
	uart_init();

	help();
	prompt();

	while (1)
	{
		console_service();
	}

	return 0;
}
