## call number 

The system call number of XV6 is different from the required one (a small pitfall), just change it to the required one  
(call write before calling sys_times here)
![times_call_number](image-29.png)

## sleep syscall
The test file seems to use ``timeval`` instead of ``timespec``
 ![struct timeval](image-30.png)


## other 
1. be careful about duplicate file names when ``mkfs``, may lead to unknown error