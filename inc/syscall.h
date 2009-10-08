#ifndef JOS_INC_SYSCALL_H
#define JOS_INC_SYSCALL_H

/* system call numbers */
enum
{
	SYS_cputs = 0,
	SYS_cgetc,
	SYS_getenvid,
	SYS_env_destroy,
    SYS_dump_env,
	NSYSCALLS
};

#endif /* !JOS_INC_SYSCALL_H */
