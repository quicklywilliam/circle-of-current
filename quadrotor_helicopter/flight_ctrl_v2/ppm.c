#include "ppm.h"

static volatile unsigned int ppm_width[8];
static volatile unsigned int ppm_offset[8];
static volatile unsigned char ppm_chan;
static volatile unsigned char ppm_highest_chan;
static volatile unsigned int ppm_last_capt;
static volatile unsigned char ppm_ovf_cnt;
static volatile unsigned char ppm_new_data;
static volatile unsigned char ppm_tx_good;

ISR(TIMER1_CAPT_vect)
{
	unsigned char ovf_cnt = ppm_ovf_cnt;
	ppm_ovf_cnt = 0;
	unsigned long t_icr = ICR1;

	// calculate total time using overflows and time difference
	signed long t = ((t_icr | 0x10000) - ppm_last_capt) & 0xFFFF;
	if(t_icr < ppm_last_capt)
	{
	        ovf_cnt--;
	}
	t += 0x10000 * ovf_cnt;

	ppm_last_capt = t_icr;

	// check sync pulse
	if(t > ticks_500us * 6)
	{
		ppm_highest_chan = ppm_chan;
		ppm_chan = 0;
		if(ppm_tx_good == 0)
		{
			ppm_tx_good = 1;
		}
	}
	else // if pulse is shorter than 3ms, then it's a servo pulse
	{
		unsigned char index = ppm_chan % 8;
		if(t >= ticks_500us && t <= ticks_500us * 5)
		{
			ppm_width[index] = t; // store time
			ppm_chan++; // next channel
			if(ppm_chan >= 4 && ppm_tx_good != 0) // last channel, data is now good, reset to first pin
			{
				ppm_tx_good = 2;
				if(ppm_chan == ppm_highest_chan)
				{
					ppm_new_data = 1;
				}
			}
		}
		else
		{
			ppm_tx_good = 0;
		}
	}
}

ISR(TIMER1_OVF_vect)
{
	ppm_ovf_cnt++;
	if(ppm_ovf_cnt > 8)
	{
		ppm_ovf_cnt = 8;
		ppm_tx_good = 0;
	}
}

void ppm_init()
{
	for(unsigned char i = 0; i < 8; i++)
	{
		ppm_width[i] = ticks_500us * 3;
	}

	ppm_tx_good = 0;
	ppm_new_data = 0;
	ppm_chan = 0;
	ppm_ovf_cnt = 0;
	ppm_last_capt = 0;
	ppm_highest_chan = 5;

	timer1_init();

	TCCR1B |= _BV(ICNC1) | _BV(ICES1);
	TIMSK1 |= _BV(ICIE1) | _BV(TOIE1);
}

unsigned char ppm_is_new_data(unsigned char c)
{
	ppm_new_data &= c;
	return ppm_new_data;
}

unsigned char ppm_highest_chan_read()
{
	return ppm_highest_chan;
}

signed int ppm_chan_width(unsigned char i)
{
	signed int r = ppm_width[i] - ppm_offset[i];
	return r;
}

void ppm_calibrate(unsigned char t)
{
	unsigned long sum[8] = {0,0,0,0,0,0,0,0};
	for(unsigned char i = 0; i < t; i++)
	{
		ppm_new_data = 0;
		while(ppm_new_data == 0);
		for(unsigned char j = 0; j < 8; j++)
		{
			sum[j] += ppm_width[j];
		}
	}
	for(unsigned char j = 0; j < 8; j++)
	{
		ppm_offset[j] = calc_multi((signed long)sum[j], 1, (signed long)t);
	}
}
