.section sigret_sec
.global __user_rt_sigreturn
__user_rt_sigreturn:
    li.d $a7, 139 # SYS_rt_sigreturn
    syscall 0                     