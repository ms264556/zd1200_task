#ifndef __MAC_PROC_H__
#define __MAC_PROC_H__

#include <linux/module.h>
#include <linux/proc_fs.h>

#define MODE_READ   0444
#define MODE_WRITE  0222
#define MODE_RW     0644

LOCAL inline int
CREATE_READ_PROC(
          struct proc_dir_entry *proc_dir
        , struct proc_dir_entry **proc_file
        , char * name
        , int (*read_proc)(char *, char**, off_t, int, int *, void*)
        )
{
    if ( ( (*proc_file)=create_proc_entry(name, MODE_READ, proc_dir))
                == NULL ) {
        return -ENOMEM;
    }
    (*proc_file)->read_proc = read_proc;
    (*proc_file)->data = NULL;

    return 0;
}

LOCAL inline int
CREATE_RDWR_PROC(struct proc_dir_entry *proc_dir
		, struct proc_dir_entry **proc_file
		, char *name
		, int (*read_proc)(char *, char**, off_t, int, int *, void*)
		, int (*write_proc)(struct file *, const char *, unsigned long , void *)
		)
{
    if ( ( (*proc_file)=create_proc_entry(name, MODE_RW, proc_dir)) == NULL ) {
        return -ENOMEM;
    }
    (*proc_file)->read_proc =  read_proc;
    (*proc_file)->write_proc =  write_proc;
    (*proc_file)->data = NULL;

    return 0;
}


#endif /* __MAC_PROC_H__ */
