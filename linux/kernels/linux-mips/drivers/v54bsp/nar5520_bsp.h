/*
 * Copyright (c) 2004-2010 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */
 //Tony Chen 03-27-2008 Initial
 //Tony Chen 12-03-2010 Fix typo

extern int oops_occurred;

int8_t nar5520_bodyguard_init(void);
int8_t nar5520_wdt_init(void);
void nar5520_load_himem(void);
void nar5520_load_oops(void);
void nar5520_save_himem(void);
void nar5520_save_oops(void);
int nar5520_oops_guard_init(void);
void wake_oops_guard_thread(void);
void nar5520_load_himem_init(void);
