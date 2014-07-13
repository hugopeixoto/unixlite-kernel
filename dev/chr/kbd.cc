#include <lib/root.h>
#include <lib/string.h>
#include <lib/ctype.h>
#include <init/ctor.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <kern/pgrp.h>
#include "condev.h"
#include "keymap.h"

void condev_t::initkbd()
{
	capslock = 0;
	numlock = 0;
	scrolllock = 0;
	ctrling = 0;
	alting = 0;
	shifting = 0;
	allocirq(&condev_t::kbdisr, this, KBDIRQ);
	softirq = allocsoftirq(&tty_t::cook, this);
}

void condev_t::kbdisr(int irqno)
{
	char code = inbp(0x60);
	(keyhook[code&0x7f])(this, code);
	char portb = inbp(0x61);
	outbp(0x80|portb, 0x61);
	outbp(portb, 0x61);
	softirq->request();
}

#define up(code) ((code)&0x80)
#define down(code) (!up(code))

void condev_t::nullkey(char code)
{
}

void condev_t::light()
{
}

void condev_t::capslockkey(char code)
{
	if (up(code))
		return;
	capslock = !capslock; 
	light();
}

void condev_t::numlockkey(char code)
{
	if (up(code))
		return;
	numlock = !numlock; 
	light();
}

void condev_t::scrollockkey(char code)
{
	if (up(code))
		return;
	scrolllock = !scrolllock; 
	light();
}

void condev_t::ctrlkey(char code)
{
	ctrling = down(code);
}

void condev_t::altkey(char code)
{
	alting = down(code);
}

void condev_t::shiftkey(char code)
{
	shifting = down(code);
}

void condev_t::charkey(char code)
{
	if (up(code))
		return;
	if (rawq.full()) {
		sysbeep();
		return;
	}
	charkey_t * k = (charkey_t*) keymap[code];
	if (ctrling && k->ctrl) {
		rawq.putc(k->ctrl);
		return;
	}
	if (capslock && isalpha(k->ascii)) {
		rawq.putc(toupper(k->ascii));
		return;
	}
	if (shifting) {
		rawq.putc(k->shift);
		return;
	}
	rawq.putc(k->ascii);
}

void condev_t::stringkey(char code)
{
	if (up(code))
		return;
	stringkey_t * k = (stringkey_t*) keymap[code];
	int len = strlen(k->ascii);
	if (rawq.space() < len) {
		sysbeep();
		return; 
	}
	rawq.putc(k->ascii, len);
}

void condev_t::numpadkey(char code)
{
	if (up(code))
		return;
	numpadkey_t * k = (numpadkey_t*) keymap[code];
	int len = numlock ? 1 : strlen(k->unlock);
	if (rawq.space() < len) {
		sysbeep();
		return;
	}
	numlock ? rawq.putc(k->lock) : rawq.putc(k->unlock, len);
}
