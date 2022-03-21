#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <signal.h>
#include <pwd.h>
#include <curses.h>
#include <limits.h>
#include <termcap.h>
#include <termios.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <assert.h>

#define ALL_SIZE 10
#define CMD_LENG 8          //指令最大长度
#define PARA_MAX 64         //参数最大长度
#define HISTORY_NUM 20
#define MAX_LINE 100
#define STD_INPUT 0
#define STD_OUTPUT 1

#define  USED        0x1
#define  IS_TASK    0x2
#define  IS_SYSTEM    0x4
#define  BLOCKED    0x8
#define TYPE_TASK    'T'
#define TYPE_SYSTEM    'S'
#define STATE_RUN    'R'

#define MAX_NR_TASKS 1023
#define SELF    ((endpoint_t) 0x8ace)
#define _MAX_MAGIC_PROC (SELF)
#define _ENDPOINT_GENERATION_SIZE (MAX_NR_TASKS+_MAX_MAGIC_PROC+1)
#define _ENDPOINT_P(e) \
((((e)+MAX_NR_TASKS) % _ENDPOINT_GENERATION_SIZE) - MAX_NR_TASKS)
#define  SLOT_NR(e) (_ENDPOINT_P(e) + 5)
#define _PATH_PROC "/proc"
#define CPUTIME(m, i) (m & (1L << (i)))
const char *cputimenames[] = { "user", "ipc", "kernelcall" };
#define NR_TASKS	  5 
#define IDLE    ((endpoint_t) -4) /* runs when no one else can run */
#define KERNEL  ((endpoint_t) -1) /* pseudo-process for IPC and scheduling */
#define CPUTIMENAMES (sizeof(cputimenames)/sizeof(cputimenames[0]))

unsigned int nr_procs, nr_tasks;


typedef int endpoint_t;
typedef uint64_t u64_t;
typedef long unsigned int vir_bytes;


char history[HISTORY_NUM][MAX_LINE];
int shm_id;
char *buff;
int history_num=0;
int k=0;
int mark=0;
int background=0;
char currentdir[20];

//list of builtin commands
char *builtinStr[]={"cd","exit","history","mytop"};

// 指令结构体
typedef struct CMD_STRUCT
{
    char *cmd[CMD_LENG]; //数组元素为字符指针每个指针指向命令的首地址。
    char cmdStr[CMD_LENG*PARA_MAX];//cmdStr存my_substring得到的子字符串
    char nextSign; // '|' or'>' or '<'
}cmdStruct;
typedef struct CMD_ALL
{
    cmdStruct cmd_all[ALL_SIZE];//定义数组包含ALL_SIZE个cmdstruct结构体
    int cmdPtr;//用以指示对应的是cmd_all的第几个命令
}cmd_all;

cmd_all *cmd_var; //结构体指针cmd_var


int nr_total=0;
//proc 结构体
struct proc {
    int p_flags;
    endpoint_t p_endpoint;
    pid_t p_pid;
    u64_t p_cpucycles[CPUTIMENAMES];
    int p_priority;
    endpoint_t p_blocked;
    time_t p_user_time;
    vir_bytes p_memory;
    uid_t p_effuid;
    int p_nice;
    char p_name[16+1];
};


struct proc *proc = NULL, *prev_proc = NULL;

///////////////////////////////
//函数声明
int my_init(void);
int my_cd(void);
int my_exit(void);
int my_readLine(char *line);
int my_subString(char *ResultString , char *str , int start , int end);
int my_splitStr(char *resultArr[] , char *str , char *split);
int my_analyCmd(char *line);
int my_builtinCmd(void);
int my_execute(void);
int my_clearCmd(cmd_all *cmd_var );
int my_history(void);
void parse_file(pid_t pid);
void parse_dir(void);
int print_memory(void);
u64_t cputicks(struct proc *p1, struct proc *p2, int timemode);
void print_procs(struct proc *proc1, struct proc *proc2, int cputimemode);
void get_procs(void);
void getkinfo(void);
int mytop();

int main()
{
    char line[MAX_LINE];
    int pid;
    buff=(char *)malloc(10240); //给指令结构体分配内存
    cmd_var = (cmd_all *)buff;
    my_init();
    while(1)
    {
        //打印当前命令
        printf("%s",currentdir);
        printf("$");
        my_readLine(line);
        my_analyCmd(line);
        //如果是内置命令，成功执行后清理进程
        if(0==my_builtinCmd())
        {
            my_clearCmd(cmd_var);
            continue;
        }
        //fork一个新进程执行program命令
        else
        {
            pid = fork();
            if(background==1)
            {
                if (pid==0)
                {//标准输出重定向到/dev/null
                    freopen("/dev/null","w",stdout);
                    my_execute();
                    
                }//父进程ignoreSIGCHLD
                signal(SIGCHLD,SIG_IGN);
                 
            }
            else
            {
                if(pid==0)
                {
                    my_execute();
                }//父进程等待子进程执行完
                waitpid(pid,NULL,0);
            }
        }
        //保证所有命令到执行完再进行clear
        sleep(1);
        my_clearCmd(cmd_var);
    }
    return 0;
}





int my_splitStr(char *resultArr[],char *str,char *split)
{
    char *token;
    token=strtok(str,split);
    int pos=0;
    while(token!=NULL)
    {
        resultArr[pos]=token;
        token=strtok(NULL,split);
        pos++;
    }
    resultArr[pos]=NULL;
    return 0;
}


int my_init(void)
{
    char dir[300];
    char *dirArray[10];
    int i;
    cmd_var->cmdPtr=0;//指针初始化
    getcwd(dir,300);//将当前工作目录的绝对路径复制到参数dir所指的内存空间中
    my_splitStr(dirArray,dir,"/");
    //找到当前目录
    for(i=0;;i++)
    {
        if(dirArray[i]==NULL)
            break;
    }
    strcpy(currentdir,dirArray[i-1]);
    return 0;
}


int my_cd(void)
{
    char dir[300];
    char *dirArray[10];
    int i;
    if(chdir(cmd_var->cmd_all[0].cmd[1])!=0) //cd到输入的path错误
    {
        perror("chdir"); //处理错误
        return -1;
    }
    else
    {
        getcwd(dir,300); //获取当前工作目录路径到buf
        my_splitStr(dirArray,dir,"/");
        for(i=0;;i++) //找到当前目录
        {
            if(dirArray[i]==NULL)
                break;
        }
        strcpy(currentdir,dirArray[i-1]);
        printf("cd succeeded\n");
        return 0;
    }
}



int my_readLine(char *line)
{
    int index=0;
    char c;
    int j=0;
    while(1)
    {
        c=getchar();
        if(c=='\n')
        {
            history[k][j]=c;
            k++;
            history_num++;
            if(strncmp(line,"history",7)==0)
                mark=line[8]-'0'; 
            line[index]='\n';
            break;
        }
        else
        {
            line[index]=c;  //感觉这句话不需要
            history[k][j]=c;
            j++;
        }
        index++;
    }
    return 0;
}

int my_exit(void)
{
    printf("exit succeeded\n");
    exit(0); //正常退出
    return 0;
}

int my_subString(char *ResultString,char *str,int start,int end)
{
    //将子字符串复制到一个char数组中
    //ResultString是目标数组的指针
    int i;
    int j=0;
    for(i=start;i<=end;i++)
    {
        ResultString[j]=str[i];
        j++;
    }
    ResultString[j]='\0';
    return 0;
}

int my_analyCmd(char *line)
{
    int previousEnd=0;
    int i;
    int j=0;
    for(i=0;line[i]!='\n';i++)
    {
        if(line[i]=='&')
            background=1;
        if(line[i]=='|' || line[i]=='<' || line[i]=='>')
        {
            my_subString(cmd_var->cmd_all[j].cmdStr,line,previousEnd,i-1);
            my_splitStr(cmd_var->cmd_all[j].cmd,cmd_var->cmd_all[j].cmdStr," ");
            cmd_var->cmd_all[j].nextSign=line[i];
            j++;
            previousEnd=i+1;
        }
    }
    //检测到一行结束 把这行命令存入subString(不包括最后的\n)
    my_subString(cmd_var->cmd_all[j].cmdStr,line,previousEnd,i-1);
    my_splitStr(cmd_var->cmd_all[j].cmd,cmd_var->cmd_all[j].cmdStr," ");
    return 0;
}


int my_history(void)
{
    int i;
    int j=0;
    for(i=history_num-mark;i<history_num;)
    {
        if(history[i][j]=='\n')
        {
            printf("\n");
            i++;
            j=0;
        }
        else
        { //在readline函数中已将字符写入history二维数组
            printf("%c",history[i][j]);
            j++;
        }
    }
    return 0;
}

int my_builtinCmd(void)
{
    if(strcmp("cd",cmd_var->cmd[0].cmd[0]))
    {
        my_cd();
        return 0;
    }
    else if(strcmp("exit",cmd_var->cmd_all[0].cmd[0])==0)
    {
        my_exit();
        return 0;
    }
    else if(strcmp("mytop",cmd_var->cmd_all[0].cmd[0])==0)
    {
        mytop();
        return 0;
    }
    else if(strcmp("histroy",cmd_var->cmd_all[0].cmd[0])==0)
    {
        my_history();
        return 0;
    }
    else
    {
        return -1;
    }
}

int my_clearCmd(cmd_all *cmd_var)
{ 
    //清空所有保存命令的数组
    int i,j;
    background=0;
    cmd_var->cmdPtr=0;
    for(i=0;i<ALL_SIZE;i++)
    {
        for(j=0;j<CMD_LENG;j++)
        {
            cmd_var->cmd_all[i].cmd[j]=NULL; //保存命令数组置NULL
            cmd_var->cmd_all[i].cmdStr[0]='\0'; //保存命令子数组置\0
        }
        cmd_var->cmd_all[i].nextSign=0;
    }
    return 0;
}

int fd[2];
int my_execute(void) //管道、重定向
{
    int pid;
    int localPtr;
    if(cmd_var->cmd_all[cmd_var->cmdPtr].nextSign=='|')
    {
        localPtr=cmd_var->cmdPtr;
        pipe(&fd[0]); //新建一个管道
        pid=fork();
        if(pid==0) //在子进程中执行下一个命令，即紧随“|”的命令
        {
            cmd_var->cmdPtr++;
            close(fd[1]); //子进程关闭写
            close(STD_INPUT);
            dup(fd[0]); //将标准输入指向fd[0]
            close(fd[0]); //不再需要该文件描述符
            //my_execute(); 
            exit(0);
        }
        else //在父进程中执行当前命令，把标准输出重定向到管道的写端
        {
            //signal(SIGCHLD,SIG_INT);
            close(fd[0]);
            close(STD_OUTPUT);
            dup(fd[1]); //创建一个文件描述符，将标准输出指向fd[1]
            close(fd[1]); //不再需要该文件描述符
            if(execvp(cmd_var->cmd_all[localPtr].cmd[0],cmd_var->cmd_all[localPtr].cmd)!=0)
                printf("No such command!\n");
            exit(0);
        }
    }
    else if(cmd_var->cmd_all[cmd_var->cmdPtr].nextSign=='>')
    { //从标准输出重定向到文件
        localPtr=cmd_var->cmdPtr;
        char fileName[20];
        strcpy(fileName,cmd_var->cmd_all[localPtr+1].cmd[0]); //把文件名复制到filename
        freopen(fileName,"w",stdout); 
        if(execvp(cmd_var->cmd_all[localPtr].cmd[0],cmd_var->cmd_all[localPtr].cmd)!=0)
            printf("No such command!\n");
        exit(0);
    }
    else if(cmd_var->cmd_all[cmd_var->cmdPtr].nextSign=='>>')
    { //从标准输出重定向到文件
        localPtr=cmd_var->cmdPtr;
        char fileName[20];
        strcpy(fileName,cmd_var->cmd_all[localPtr+1].cmd[0]); //把文件名复制到filename
        freopen(fileName,"a+",stdout); 
        if(execvp(cmd_var->cmd_all[localPtr].cmd[0],cmd_var->cmd_all[localPtr].cmd)!=0)
            printf("No such command!\n");
        exit(0);
    }
    else if(cmd_var->cmd_all[cmd_var->cmdPtr].nextSign=='<')
    { //从标准输出重定向到文件
        localPtr=cmd_var->cmdPtr;
        char fileName[20];
        strcpy(fileName,cmd_var->cmd_all[localPtr+1].cmd[0]); //把文件名复制到filename
        freopen(fileName,"r",stdin); 
        if(execvp(cmd_var->cmd_all[localPtr].cmd[0],cmd_var->cmd_all[localPtr].cmd)==0)
            printf("No such command!\n");
        exit(0);
    }
    else
    { //如果没有重定向和管道，先保存当前的localPtr
        localPtr=cmd_var->cmdPtr;
        //执行命令 execvp第一个是命令第二个是参数表，如果返回0就没有这个命令
        if(execvp(cmd_var->cmd_all[localPtr].cmd[0],cmd_var->cmd_all[localPtr].cmd)!=0)
            printf("No such command!\n");
        exit(0);
    }
    return 0;
}



//u64_t 64位 high和low32位 拼接成64位 high+low
static inline u64_t make64(unsigned long lo, unsigned long hi)
{
    return ((u64_t)hi << 32) | (u64_t)lo;
}

void parse_file(pid_t pid) //pid_t是对进程号的类型定义，其实是个int
{
    char path[PATH_MAX],name[256],type,state; // path，name，类型，状态
    int version,endpt,effuid; //版本，端点，有效用户id
    unsigned long cycles_hi,cycles_lo; //高周期，低周期
    FILE *fp;
    struct proc *p;
    int slot;
    int i;
    
    sprintf(path,"/proc/%d/psinfo",pid); //sprintf发送格式化输出到path所指向的字符串。
    if((fp=fopen(path,"r"))==NULL) //按照/proc/%d/psinfo打开path中的文件
        return;
    if (fscanf(fp, "%d", &version) != 1)  //判断version是否为1，如果不是该进程不需要记录
    {
        fclose(fp);
        return;
    }
    if (version!=0) //version错误处理 
    {
        fputs("procfs version mismatch!\n", stderr);
        exit(1);
    }
    if(fscanf(fp," %c %d",&type,&endpt)!=2) //fscanf从fp中读取格式化输入 注意空格
    { //读入类型和端点，判断是否读入的是两个
        fclose(fp);
        return;
    }
    slot=SLOT_NR(endpt);
    slot++; //确保不要重复
    //判断endpoint的值是否合理 在0到nr_total的范围内
    if(slot < 0 || slot >= nr_total) {
        //fprintf(stderr, "top: unreasonable endpoint number %d\n", endpt);
        fclose(fp);
        return;
    }
     //slot为该进程结构体在数组中的位置
    p = &proc[slot];//把slot地址赋值给p
    if(type==TYPE_TASK)
        p->p_flags|=IS_TASK; //标记task进程
    else if(type==TYPE_SYSTEM)
        p->p_flags|=IS_SYSTEM; //标记system进程
    p->p_endpoint=endpt; //存入对应进程的结构体
    p->p_pid=pid;
    if (fscanf(fp, " %255s %c %d %d %lu %*u %lu %lu",
        name, &state, &p->p_blocked, &p->p_priority,
        &p->p_user_time, &cycles_hi, &cycles_lo) != 7) 
    { //读入名字 状态 阻塞状态 动态优先级 进程时间 高周期 低周期
        fclose(fp);
        return;
    }
    strncpy(p->p_name, name, sizeof(p->p_name)-1);
    p->p_name[sizeof(p->p_name)-1] = 0;
    if (state != STATE_RUN)
		p->p_flags |= BLOCKED; //若不是run的进程，标记blocked
	p->p_cpucycles[0] = make64(cycles_lo, cycles_hi); //拼接成64位，放在p_cpucycles[]数组中
	p->p_memory = 0L;
    if (!(p->p_flags & IS_TASK)) 
    {
		int j;
		if ((j=fscanf(fp, " %lu %*u %*u %*c %*d %*u %u %*u %d %*c %*d %*u",&p->p_memory, &effuid, &p->p_nice)) != 3) 
        {
        fclose(fp);
        return;
        }
        p->p_effuid=effuid;
	} 
    else 
        p->p_effuid=0;
    for(i = 1; i < CPUTIMENAMES; i++) 
    { //连续读CPUTIMENAMES次cycles_hi、cycles_lo，然后拼接成64位，放在p_cpucycles数组中
		if(fscanf(fp, " %lu %lu",&cycles_hi, &cycles_lo) == 2) 
        {
            p->p_cpucycles[i] = make64(cycles_lo, cycles_hi);
		} 
        else	
        {
			p->p_cpucycles[i] = 0;
		}
	}
	if ((p->p_flags & IS_TASK)) //读入内存 存入对应进程的结构体
    {
		if(fscanf(fp, " %lu", &p->p_memory)!=1) 
        {
			p->p_memory = 0;
		}
	}
	p->p_flags |= USED; //按位或
	fclose(fp);
}



void parse_dir(void)
{
    DIR *p_dir;
    struct dirent *p_ent; //头文件dirent.h
    pid_t pid;
    char *end;
    //打开/proc
    if ((p_dir = opendir("/proc/")) == NULL) {
        perror("opendir on /proc");
        exit(1);
    }
    p_ent=readdir(p_dir); //readdir()返回参数p_dir 目录流的下个目录进入点。
    for (p_ent = readdir(p_dir); p_ent != NULL; p_ent = readdir(p_dir)) 
    {
		pid = strtol(p_ent->d_name, &end, 10); //strtol将字符串转化为十进制数
		if (!end[0] && pid != 0)
			parse_file(pid);
	}
    closedir(p_dir);
}

int print_memory(void)
{
    FILE *fp;
    unsigned int pagesize;
    unsigned long total, free, largest, cached;
    //打开meminfo
    if ((fp = fopen("/proc/meminfo", "r")) == NULL)
        return 0;
    //读输入
    if (fscanf(fp, "%u %lu %lu %lu %lu", &pagesize, &total, &free,&largest, &cached) != 5) 
    {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    //打印总体内存、空闲内存、缓存cache大小
    printf("main memory: %ldK total, %ldK free, %ldK contig free, "
        "%ldK cached\n",(pagesize * total)/1024, (pagesize * free)/1024,
        (pagesize * largest)/1024, (pagesize * cached)/1024);
    return 1;
}

//结构体tp，对应某个进程和滴答
struct tp 
{
    struct proc *p;
    u64_t ticks;
};

//滴答并不是简单的结构体中的滴答，因为在写文件的时候需要更新。需要通过当前进程来和该进程一起计算

u64_t cputicks(struct proc *p1, struct proc *p2, int timemode)
{
    int i;
    u64_t t = 0;
    //计算每个进程proc的滴答，通过proc和当前进程prev_proc做比较，如果endpoint相等，则在循环中分别计算
    for(i = 0; i < CPUTIMENAMES; i++) 
    {
        if(!CPUTIME(timemode, i))
            continue;
        if(p1->p_endpoint == p2->p_endpoint) 
        {
            t = t + p2->p_cpucycles[i] - p1->p_cpucycles[i];
        } 
        else 
        {
            t = t + p2->p_cpucycles[i];
        }
    }
    return t;
}

void print_procs(struct proc *proc1, struct proc *proc2, int cputimemode)
{
	int p, nprocs;
	u64_t idleticks = 0;
	u64_t kernelticks = 0;
	u64_t systemticks = 0;
	u64_t userticks = 0;
	u64_t total_ticks = 0;
	int blockedseen = 0;
	static struct tp *tick_procs = NULL;
	if (tick_procs == NULL) 
    {   
		tick_procs = malloc(nr_total * sizeof(tick_procs[0])); //创建tick_procs并分配内存
		if (tick_procs == NULL) 
        {
			fprintf(stderr, "Out of memory!\n");
			exit(1);
		}
	}
	for(p = nprocs = 0; p < nr_total; p++) //对所有进程进行遍历
    {
		u64_t uticks;
		if(!(proc2[p].p_flags & USED)) //若当前进程的标记不是used，continue
			continue;
		tick_procs[nprocs].p = proc2 + p;
		tick_procs[nprocs].ticks = cputicks(&proc1[p], &proc2[p], cputimemode);
		uticks = cputicks(&proc1[p], &proc2[p], 1); //uticks实时更新
		total_ticks = total_ticks + uticks; //total_ticks实时更新
		//kernelticks和idleticks为0
        if(p-NR_TASKS == IDLE) //IDLE 空闲时间，它不包括等待 I/O 的时间（iowait）
        {
			idleticks = uticks;
			continue;
		}
		if(p-NR_TASKS == KERNEL) 
        {
			kernelticks = uticks;
		}
        //判断是否为systemtick和usertick
		if(!(proc2[p].p_flags & IS_TASK)) {
			if(proc2[p].p_flags & IS_SYSTEM)
				systemticks = systemticks + tick_procs[nprocs].ticks; //系统进程加到systemticks
			else
				userticks = userticks + tick_procs[nprocs].ticks; //用户进程加到userticks 
		}

		nprocs++; //计数器++
	}
    printf("%d\n",total_ticks);
	if (total_ticks == 0) //若所有滴答为0，返回
		return;
    //打印
	printf("CPU states: %6.2f%% user, ", 100.0 * userticks / total_ticks);
	printf("%6.2f%% system, ", 100.0 * systemticks / total_ticks);
	printf("%6.2f%% kernel, ", 100.0 * kernelticks/ total_ticks);
	printf("%6.2f%% idle", 100.0 * idleticks / total_ticks);
    printf("%6.2f%% total, ", 100.0-100.0 *(userticks+systemticks+ kernelticks)/total_ticks);
    printf("\n");
}


//get_procs将所有需要的信息放在结构体数组proc[]中，每个元素都是一个进程结构体。
void get_procs(void)
{
    struct proc *p;
    int i;
    p = prev_proc; 
    prev_proc = proc;  //记录当前进程，赋值给prev_proc，proc是个全局变量
    proc = p;
    if (proc == NULL) {
        //分配内存，每个进程分配一个结构体，分配nr_total个单位proc结构体内存空间,并让proc指针指向该空间
        proc = malloc(nr_total * sizeof(proc[0]));
        //错误处理
        if (proc == NULL) {
            fprintf(stderr, "Out of memory!\n");
            exit(1);
        }
    }
    //先将所有flag置0
    for (i = 0; i < nr_total; i++)
        proc[i].p_flags = 0;
    //调用parse_dir分析pid
    parse_dir();
}

void getkinfo(void)
{
    FILE *fp;
    if ((fp = fopen("/proc/kinfo", "r")) == NULL) 
    {        
        exit(1);
    }
    //读如nr_procs,nr_tasks
    if (fscanf(fp, "%u %u", &nr_procs, &nr_tasks) != 2) {
       
        exit(1);
    }
    fclose(fp);
    //算出总的nr_total
    nr_total = (int) (nr_procs + nr_tasks);
}

int mytop()
{
    //跳转到/proc
    if (chdir("/proc") != 0) 
    {
        perror("chdir to /proc" );
        return 1;
    }
    print_memory();
    getkinfo();
    get_procs();
    //当前进程为空的话 就要再调用get_procs
    if(prev_proc==NULL)
        get_procs();
    print_procs(prev_proc,proc,1);
    return 0;
}





















