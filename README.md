# OS_shell
Project 1
1.安装MINIX操作系统（Version3.3）
2.学校Shell，系统编程，实现一个基本的shell
要求Shell能解析如下命令
1.带参数的程序运行功能。
  program arg1 arg2 … argN
2. 重定向功能，将文件作为程序的输入/输出。
  1. “>”表示覆盖写
    program arg1 arg2 … argN > output-file
  2. “>>”表示追加写
    program arg1 arg2 … argN >> output-file
  3. “<”表示文件输入
    program arg1 arg2 … argN < input-file
3. 管道符号“|”，在程序间传递数据。
  programA arg1 … argN | programB arg1 … argN
4. 后台符号& ,表示此命令将以后台运行的方式执行。
  program arg1 arg2 … argN &
5. 工作路径移动命令cd。
6. 程序运行统计mytop。
7. shell退出命令exit。
8. history n显示最近执行的n条指令


## Shell内置命令：
1. cd：因为Shell也是一个程序，启动时minix会分配一个当前工作目录，利用chdir系统调用可以移动Shell的工作目录。
2. history：保存Shell每次的输入行，打印所需字符串即可。
3. exit：退出Shell的while循环，结束Shell的main函数。
4. mytop：参考minix终端输入top命令的输出信息，在minix系统/proc文件夹中通过open/read系统调用输出进程信息。
  1. /proc/meminfo中，查看内存信息，每个参数对应含义依次是页面大小pagesize，总页数量total ，空闲页数量free，最大页数量largest ，缓存页数量cached。可计算内存大小：(pagesize * total)/1024，同理算出其他页内存大小。
  2. /proc/kinfo中，查看进程和任务数量。
  3. /proc/pid/psinfo中，例如/proc/107/psinfo文件中，查看pid为107的进程信息。每个参数对应含义依次是：版本version，类型type，端点endpt，名字name，状态state，阻塞状态blocked，动态优先级priority，滴答ticks，高周期highcycle，低周期lowcycle，内存memory，有效用户ID effuid，静态优先级nice等。其中会用到的参数有：类型，状态，滴答。进程时间time=ticks/ (u32_t)60。
  4. 输出内容：
    1. 总体内存大小，空闲内存大小，缓存大小。
    2. 总体CPU使用占比。计算方法：得到进程和任务总数量total_proc，对每一个proc的ticks累加得到总体ticks，再计算空闲的ticks，最终可得到CPU使用百分比。

## Program命令：
1. 运行程序：利用fork调用创建进行子进程，利用execvp调用将minix程序装载到该进程，并赋予运行参数，最后Shell利用wait/waitpid调用等待子进程结束。（参见UNIX高级编程8.3，8.7和8.10节）
2. 重定向：minix为每个进程赋予键盘输入和控制台输出的文件描述符默认为0和1。子进程装载程序前，利用close(0 or 1)将默认输入或者输出关闭，再调用dup(fd)将某个打开文件的文件描述fd映射到标准输入或输出。（参见UNIX高级编程3.12节）
3. 管道：若有n个子进程组成管道流，Shell在fork前先用pipe调用创建n-1对管道描述符，关闭不需要的读写端。Shell运行fork后，每个子进程利用dup将前一个管道的读端映射到标准输入，后一个管道的写端映射到标准输出。（参见UNIX高级编程15.2节）
4. 后台运行：为了屏蔽键盘和控制台，子进程的标准输入、输出映射成/dev/null。子进程调用signal(SIGCHLD,SIG_IGN)，使得minix接管此进程。因此Shell可以避免调用wait/waitpid直接运行下一条命令。

## 参考资料
1. 在实现管道时，需要用到pipe和dup系统调用等，重定向标准向输入输出(参见教材1.4.3节)。
2. Shell主要是为用户提供一个命令解释器，接收用户命令（如ls等)，然后调用相应的应用程序。实现的Shell支持后台进程的运行。
3. 在计算内存和CPU总体使用情况时，可参考top命令实现https://github.com/0xffea/MINIX3/blob/master/usr.bin/top/top.c
4. 推荐《UNIX环境高级编程》查找详细的系统调用使用方法。
