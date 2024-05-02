/*
 *  * Copyright (c) 2004-2010 Ruckus Wireless, Inc.
 *   * All rights reserved.
 *    *
 *     */
 //Tony Chen    2010-12-27   Initial
 //
#ifndef __NAR5520_PROC_H__
#define __NAR5520_PROC_H__

struct entry_table
{
   const char *entry_name;
   struct proc_dir_entry *proc_entry;
   int (*read_proc)(char *, char**, off_t, int, int *, void*);
   int (*write_proc)(struct file *, const char *, unsigned long , void *);
   struct file_operations *fop_proc;
   void *data;
};

#endif
