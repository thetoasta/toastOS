/*
 * toastOS++ Master Header
 * Copyright (C) 2025 thetoasta
 * Licensed under MPL-2.0
 * 
 * NAMESPACE STRUCTURE:
 * ====================
 * 
 * toast::mem      - Memory management
 *                   toast::mem::alloc(1024)
 *                   toast::mem::free(ptr)
 *                   toast::mem::create<MyStruct>()
 * 
 * toast::fs       - Filesystem (FAT16)
 *                   toast::fs::create("/hello.txt", "Hello!")
 *                   toast::fs::read("/file.txt", buf, size)
 *                   toast::fs::mkdir("/mydir")
 * 
 * toast::io       - Input/Output (keyboard, screen)
 *                   toast::io::print("Hello!")
 *                   toast::io::println("Line")
 *                   toast::io::input()
 *                   toast::io::clear()
 * 
 * toast::gfx      - Graphics (double-buffered)
 *                   toast::gfx::init(fb, w, h, pitch)
 *                   toast::gfx::rect(x, y, w, h, color)
 *                   toast::gfx::text(x, y, "Hello", 0xFFFFFF)
 *                   toast::gfx::flush()
 * 
 * toast::sys      - System (panic, init, IDT)
 *                   toast::sys::panic("Error!")
 *                   toast::sys::warn("Warning")
 *                   toast::sys::init()
 * 
 * toast::disk     - Disk I/O (ATA/IDE)
 *                   toast::disk::init()
 *                   toast::disk::read(lba, count, buf)
 *                   toast::disk::write(lba, count, buf)
 * 
 * toast::net      - Networking (RTL8139)
 *                   toast::net::init()
 *                   toast::net::ping("10.0.2.2")
 *                   toast::net::http_get("example.com", "/")
 * 
 * toast::thread   - Threading
 *                   toast::thread::create("name", func, arg)
 *                   toast::thread::yield()
 *                   toast::thread::sleep(100)
 *                   toast::thread::mutex::lock(&m)
 * 
 * toast::time     - Time & Alarms
 *                   toast::time::now()
 *                   toast::time::uptime()
 *                   toast::time::alarm::set(12, 30, "Lunch")
 * 
 * toast::user     - User Management
 *                   toast::user::login("admin", "pass")
 *                   toast::user::logout()
 *                   toast::user::current()
 * 
 * toast::reg      - Registry (key-value store)
 *                   toast::reg::set("KEY", "value")
 *                   toast::reg::get("KEY")
 *                   toast::reg::save()
 * 
 * toast::app      - Application Layer
 *                   toast::app::register_app("myapp", PERM_ALL)
 *                   toast::app::print("Hello from app")
 *                   toast::app::exit(0)
 * 
 * Legacy C-style functions remain available for compatibility:
 *   kmalloc(), kfree(), kprint(), fat16_init(), etc.
 */

#ifndef TOAST_HPP
#define TOAST_HPP

/* Core headers */
#include "stdint.hpp"
#include "mmu.hpp"
#include "fat16.hpp"
#include "kio.hpp"
#include "graphics.hpp"
#include "panic.hpp"
#include "ata.hpp"
#include "net.hpp"
#include "thread.hpp"
#include "time.hpp"
#include "user.hpp"
#include "registry.hpp"

#endif /* TOAST_HPP */
