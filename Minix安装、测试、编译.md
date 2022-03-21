# Minix安装、测试、编译

Minix，是一个迷你版本的类Unix操作系统。它的名称取自英语：Mini UNIX的缩写，作为授课及学习之用。

## 安装

1. 从[Minix3](http://www.minix3.org/)官网下载iso镜像文件（镜像文件不要双击
2. 下载虚拟机VMware[workstation](https://store-us.vmware.com/black-friday-2021-savings-event?irclickid=3nZRBZWUnxyITocyqFXJXxllUkGTTwWTm02HxQ0&utm_source=affiliate&utm_medium=ONLINE_TRACKING_LINK_&utm_campaign=Online Tracking Link&utm_term=_DGMAX Interactive&irpid=12796&irgwc=1)
3. 下载[MobaXterm free Xserver and tabbed SSH client for Windows (mobatek.net)](https://mobaxterm.mobatek.net/)方便我们能用物理机连接虚拟机

## 配置

1. 在VMware中设置minix的各种参数大小，以及一些默认配置。可以输入一些基本命令看是否安装成功，比如ls
2. 配置开发环境（minix的网络连接为NAT模式
   - pkgin update
   - pkgin install git-base
   - pkgin install openssh
   - pkgin install vim
   - pkgin install clang
   - pkgin install binutils
3. 通过SSH远程连接
   1. minix的网络连接改为桥接/主机模式
   2. 在minix中查看本机ip地址，并且设置SSH密码
   3. 在mobaxterm中用SSH连接，输入账号密码即可。这一步如果一直报Permission Denied错误，可以考虑从已经连接成功的人那里拷贝他们的虚拟机文件或者重新装一遍虚拟机。如果出现/etc/ssh_config line1:Missing argument 可以通过vim /etc/ssh_config 命令，将User那一行加入用户名root（这个错误每次重启moba都会发生，我也不知道如何一劳永逸地解决这个错误，每次打开moba都vim一下吧。。。
4. 现在可以通过clang来编译某个c程序，看看是否配置成功 比如clang hello.c -o hello

## 编译

1. clang aaa.c -o bbb
2. /bbb

ps编译需要在文件所在的目录进行编译