/*
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 * You may obtain a copy of the License at https://mozilla.org/MPL/2.0/
 */

#include "drivers/kio.h"
#include "drivers/security.h"
#include "drivers/services.h"
#include "drivers/rtc.h"
#include "drivers/registry.h"
#include "drivers/disk.h"

/* there are 25 lines each of 80 columns; each element takes 2 bytes */
void kmain(void)
{	
	clear_screen();
	disk_init();
	rtc_init();
	registry_init();
	registry_load();
	init_services();
	security_init();
	init_shell();
	
	// Initialize and test the filesystem

	while(1);
}