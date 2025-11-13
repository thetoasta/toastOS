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
#include "drivers/file.h"
#include "drivers/timer.h"
#include "drivers/editor.h"
#include "drivers/apps.h"
#include "drivers/network.h"
#include "drivers/arp.h"

/* there are 25 lines each of 80 columns; each element takes 2 bytes */
void kmain(void)
{	
	clear_screen();
	serial_init();
	serial_write_string("\n========================================\n");
	serial_write_string("   toastOS Kernel Initialization\n");
	serial_write_string("========================================\n");
	
	serial_write_string("[KERNEL] Initializing disk driver...\n");
	disk_init();
	
	serial_write_string("[KERNEL] Initializing RTC (Real-Time Clock)...\n");
	rtc_init();
	
	serial_write_string("[KERNEL] Initializing registry...\n");
	registry_init();
	
	serial_write_string("[KERNEL] Loading registry from disk...\n");
	registry_load();
	
	serial_write_string("[KERNEL] Initializing filesystem...\n");
	fs_init();
	
	serial_write_string("[KERNEL] Initializing application system...\n");
	apps_init();
	
	serial_write_string("[KERNEL] Initializing security subsystem...\n");
	security_init();
	
	serial_write_string("[KERNEL] Initializing timer...\n");
	timer_init();
	
	serial_write_string("[KERNEL] Initializing network stack...\n");
	network_init();
	arp_init();
	
	serial_write_string("[KERNEL] Registering autosave callback (10 seconds)...\n");
	timer_register_callback(editor_autosave_callback, 100);  // 100 ticks = 10 seconds
	
	serial_write_string("[KERNEL] Starting shell...\n");
	init_shell();
	
	serial_write_string("[KERNEL] Entering main loop (system ready)\n");
	// Initialize and test the filesystem

	// Main kernel loop with network polling
	while(1) {
		// Poll for network packets
		network_poll();
		
		// Halt CPU until next interrupt (saves power)
		asm volatile("hlt");
	}
}