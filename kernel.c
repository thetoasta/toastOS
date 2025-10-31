/*
* Copyright (C) 2014  Arjun Sreedharan
* License: GPL version 2 or higher http://www.gnu.org/licenses/gpl.html
*/

#include "drivers/kio.h"

/* there are 25 lines each of 80 columns; each element takes 2 bytes */
void kmain(void)
{
	clear_screen();
	kprint("hey yall");
	kprint_newline();
	kprint_newline();

	idt_init();
	kb_init();

	while(1);
}