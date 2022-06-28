# init组件<a name="ZH-CN_TOPIC_0000001129033057"></a>

- [init组件<a name="ZH-CN_TOPIC_0000001129033057"></a>](#init组件)
  - [简介<a name="section469617221261"></a>](#简介)
  - [目录<a name="section15884114210197"></a>](#目录)
  - [约束<a name="section12212842173518"></a>](#约束)
  - [使用说明<a name="section837771600"></a>](#使用说明)
  - [相关仓<a name="section641143415335"></a>](#相关仓)

## 简介<a name="section469617221261"></a>

init组件负责处理从内核加载第一个用户态进程开始，到第一个应用程序启动之间的系统服务进程启动过程。启动恢复子系统除负责加载各系统关键进程之外，还需在启动的同时设置其对应权限，并在子进程启动后对指定进程实行保活（若进程意外退出要重新启动），对于特殊进程意外退出时，启动恢复子系统还要执行系统复位操作。

## 目录<a name="section15884114210197"></a>

```
base/startup/init_lite/          # init组件
├── device_info
├── initsync
├── interfaces                   # init提供的对外接口
├── scripts
├── services
    ├── begetctl                 # init归一化命令
    ├── etc                      # init配置文件目录（标准系统）
    ├── etc_lite                 # init配置文件目录（小型系统）
    ├── include                  # init头文件目录
    ├── init                     # init核心功能源码
    ├── log                      # init日志部件
    ├── loopevent                # init提供的小型异步事件库
    ├── modules                  # 插件化模块
    ├── param                    # 系统参数部件
    ├── utils                    # init工具库
    └── BUILD.gn
├── test                         # init组件测试用例源文件目录
├── ueventd                      # ueventd服务源码
├── watchdog                     # 看门狗服务源码
├── begetd.gni
├── bundle.json
├── LICENSE
├── README_ZH.md
├── README.md
└── OAT.xml

device
└──hihope
        └──rk3568
                └──build
                       └──rootfs  # init配置文件目录（适用于rk3568平台）

vendor
└──hisilicon
        └──hispark_taurus_linux
                       └──init_configs  # init配置文件目录（适用于L1 Linux内核版本）
```

## 约束<a name="section12212842173518"></a>

目前支持小型系统设备（参考内存≥1MB），如Hi3516DV300、Hi3518EV300以及RK3568等

## 使用说明<a name="section837771600"></a>

init将系统启动分为三个阶段：

“pre-init”阶段：启动系统服务之前需要先执行的操作，例如挂载文件系统、创建文件夹、修改权限等

“init”阶段：系统服务启动阶段

“post-init”阶段：系统服务启动完后还需要执行的操作

上述每个阶段在配置文件init.cfg中都用一个job表示，每个job都对应一个命令集合，init通过依次执行每个job中的命令来完成系统初始化。job执行顺序：先执行“pre-init”，再执行“init”，最后执行“post-init”，所有job都集中放在init.cfg的jobs数组中。

除上述jobs数组之外，init.cfg中还有一个services数组，用于存放所有需要由init进程启动的系统关键服务的服务名、可执行文件路径、权限和其他属性信息。

对于每个service的启动，从init拉起的方式上来区分，大致可分为以下三种策略：

* 通过start命令
在job中添加start service的命令，init将会在执行该job的阶段将对应服务拉起
* 分组并行启动
无须显式添加start命令，服务的start-mode属性为非condition配置，init将会为该服务按策略分组并在该分组服务启动时统一拉起
* 按需启动
按需启动的服务应当被认为是无须在系统启动过程中被拉起的，而是当需要时，这个当需要时的触发条件可能是被init监听的相关socket有消息上报、samgr收到客户端的请求需要拉起SA服务等情况，按需启动的服务需要配置ondemand属性为true，该属性拥有高优先级，配置该属性后服务将不再受start-mode属性控制，统一通过按需启动方式拉起

对于每个服务的启动，进程的运行，init提供了以保障系统安全性为目的的沙盒运行环境。每个进程运行时都有不同的环境约束，各个分层之间进程的资源隔离，确保每个进程都在各自的沙盒环境下运行，只访问允许的系统资源。

每个沙盒环境的分为只读资源和可写资源，只读资源由init在初始化时创建好，通过mount bind把只读文件指向全局FS中对应的目录，然后启动相应沙盒进程时通过SetNamespace跳入到沙盒环境运行。对于可写目录，通过对全局/data目录进行划分，由存储服务进行统一管理分配，通过mnt namespace完成可写目录的沙盒化。

init的关键配置文件init.cfg位于代码仓库base/startup/init_lite/service/etc目录，部署在/etc/下，采用json格式，文件大小目前限制在100KB以内。

配置文件格式和内容说明如下所示：

```
{
    "jobs" : [{
            "name" : "pre-init",
            "cmds" : [
                "mkdir /testdir",
                "chmod 0700 /testdir",
                "chown 99 99 /testdir",
                "mkdir /testdir2",
                "mount vfat /dev/mmcblk0p0 /testdir2 noexec nosuid"
            ]
        }, {
            "name" : "init",
            "cmds" : [
                "copy /testfile /testfile2",
                "symlink /testfile /testlink",
                "write /testfile 0",
                "ifup lo",
                "hostname testhost",
                "domainname testdomain"
            ]
        }, {
            "name" : "post-init",
            "cmds" : [
                "trigger testjob",
                "trigger testjob2"
            ]
        }, {
            "name" : "services:service2",
            "cmds" : [
                "chmod 0773 /data/service2"
            ]
        }
    ],
    "services" : [{
            "name" : "service1",
            "path" : ["/system/bin/process1"],
            "socket" : [{
                "name" : "process1",
                "family" : "AF_NETLINK",
                "type" : "SOCK_DGRAM",
                "protocol" : "NETLINK_KOBJECT_UEVENT",
                "permissions" : "0660",
                "uid" : "system",
                "gid" : "system",
                "option" : [
                    "SOCKET_OPTION_PASSCRED",
                    "SOCKET_OPTION_RCVBUFFORCE"
                ]
            }],
            "critical" : [ 0, 15, 5],
            "ondemand" : true
        }, {
            "name" : "service2",
            "path" : ["/system/bin/process2"],
            "start-mode" : "condition",
            "disabled" : 1,
            "console" : 1,
            "uid" : "root",
            "gid" : ["shell", "log", "readproc"],
            "jobs" : {
                "on-start" : "services:service2"
            }
        }
    ]
}
```

**表 1**  执行job介绍

<a name="table1801509284"></a>
<table><thead align="left"><tr id="row680703289"><th class="cellrowborder" valign="top" width="13.4%" id="mcps1.2.3.1.1"><p id="p11805012282"><a name="p11805012282"></a><a name="p11805012282"></a>job名</p>
</th>
<th class="cellrowborder" valign="top" width="86.6%" id="mcps1.2.3.1.2"><p id="p2811605289"><a name="p2811605289"></a><a name="p2811605289"></a>说明</p>
</th>
</tr>
</thead>
<tbody><tr id="row178140112810"><td class="cellrowborder" valign="top" width="13.4%" headers="mcps1.2.3.1.1 "><p id="p6811809281"><a name="p6811809281"></a><a name="p6811809281"></a>pre-init</p>
</td>
<td class="cellrowborder" valign="top" width="86.6%" headers="mcps1.2.3.1.2 "><p id="p18115019284"><a name="p18115019284"></a><a name="p18115019284"></a>最先执行的job，如果开发者的进程在启动之前需要首先执行一些操作（例如创建文件夹），可以把操作放到pre-init中先执行。</p>
</td>
</tr>
<tr id="row381120182817"><td class="cellrowborder" valign="top" width="13.4%" headers="mcps1.2.3.1.1 "><p id="p148116002812"><a name="p148116002812"></a><a name="p148116002812"></a>init</p>
</td>
<td class="cellrowborder" valign="top" width="86.6%" headers="mcps1.2.3.1.2 "><p id="p14818016288"><a name="p14818016288"></a><a name="p14818016288"></a>中间执行的job，例如服务启动。</p>
</td>
</tr>
<tr id="row181100162813"><td class="cellrowborder" valign="top" width="13.4%" headers="mcps1.2.3.1.1 "><p id="p3811804281"><a name="p3811804281"></a><a name="p3811804281"></a>post-init</p>
</td>
<td class="cellrowborder" valign="top" width="86.6%" headers="mcps1.2.3.1.2 "><p id="p18116016285"><a name="p18116016285"></a><a name="p18116016285"></a>最后被执行的job，如果开发者的进程在启动完成之后需要有一些处理（如驱动初始化后再挂载设备），可以把这类操作放到该job执行。</p>
</td>
</tr>
</tbody>
</table>

单个job最多支持30条命令（当前仅支持start/mkdir/chmod/chown/mount/loadcfg），命令名称和后面的参数（参数长度≤128字节）之间有且只能有一个空格。

**表 2**  命令集说明

<a name="table122681439144112"></a>
<table><thead align="left"><tr id="row826873984116"><th class="cellrowborder" valign="top" width="10.15%" id="mcps1.2.4.1.1"><p id="p826833919412"><a name="p826833919412"></a><a name="p826833919412"></a>命令</p>
</th>
<th class="cellrowborder" valign="top" width="34.089999999999996%" id="mcps1.2.4.1.2"><p id="p3381142134118"><a name="p3381142134118"></a><a name="p3381142134118"></a>命令格式和示例</p>
</th>
<th class="cellrowborder" valign="top" width="55.76%" id="mcps1.2.4.1.3"><p id="p1268539154110"><a name="p1268539154110"></a><a name="p1268539154110"></a>说明</p>
</th>
</tr>
</thead>
<tbody><tr id="row142681039124116"><td class="cellrowborder" valign="top" width="10.15%" headers="mcps1.2.4.1.1 "><p id="p2083714604313"><a name="p2083714604313"></a><a name="p2083714604313"></a>mkdir</p>
</td>
<td class="cellrowborder" valign="top" width="34.089999999999996%" headers="mcps1.2.4.1.2 "><p id="p143811842154111"><a name="p143811842154111"></a><a name="p143811842154111"></a>mkdir 目标文件夹</p>
<p id="p4377174213435"><a name="p4377174213435"></a><a name="p4377174213435"></a>如：mkdir /storage/myDirectory</p>
</td>
<td class="cellrowborder" valign="top" width="55.76%" headers="mcps1.2.4.1.3 "><p id="p56817536457"><a name="p56817536457"></a><a name="p56817536457"></a>创建文件夹命令，mkdir和目标文件夹之间有且只能有一个空格。</p>
</td>
</tr>
<tr id="row1268133919413"><td class="cellrowborder" valign="top" width="10.15%" headers="mcps1.2.4.1.1 "><p id="p97961563461"><a name="p97961563461"></a><a name="p97961563461"></a>chmod</p>
</td>
<td class="cellrowborder" valign="top" width="34.089999999999996%" headers="mcps1.2.4.1.2 "><p id="p20381144234118"><a name="p20381144234118"></a><a name="p20381144234118"></a>chmod 权限 目标</p>
<p id="p6334213124413"><a name="p6334213124413"></a><a name="p6334213124413"></a>如：chmod 0600 /storage/myFile.txt</p>
<p id="p1748214543444"><a name="p1748214543444"></a><a name="p1748214543444"></a>chmod 0750 /storage/myDir</p>
</td>
<td class="cellrowborder" valign="top" width="55.76%" headers="mcps1.2.4.1.3 "><p id="p2023822074614"><a name="p2023822074614"></a><a name="p2023822074614"></a>修改权限命令，chmod 权限 目标  之间间隔有且仅有一个空格，权限必须为0xxx格式。</p>
</td>
</tr>
<tr id="row7268143918416"><td class="cellrowborder" valign="top" width="10.15%" headers="mcps1.2.4.1.1 "><p id="p8255346174610"><a name="p8255346174610"></a><a name="p8255346174610"></a>chown</p>
</td>
<td class="cellrowborder" valign="top" width="34.089999999999996%" headers="mcps1.2.4.1.2 "><p id="p238114423418"><a name="p238114423418"></a><a name="p238114423418"></a>chown uid gid 目标</p>
<p id="p1118592184518"><a name="p1118592184518"></a><a name="p1118592184518"></a>如：chown 900 800 /storage/myDir</p>
<p id="p1235374884510"><a name="p1235374884510"></a><a name="p1235374884510"></a>chown 100 100 /storage/myFile.txt</p>
</td>
<td class="cellrowborder" valign="top" width="55.76%" headers="mcps1.2.4.1.3 "><p id="p18408185817467"><a name="p18408185817467"></a><a name="p18408185817467"></a>修改属组命令，chown uid gid 目标  之间间隔有且仅有一个空格。</p>
</td>
</tr>
<tr id="row109751379478"><td class="cellrowborder" valign="top" width="10.15%" headers="mcps1.2.4.1.1 "><p id="p1017823174717"><a name="p1017823174717"></a><a name="p1017823174717"></a>mount</p>
</td>
<td class="cellrowborder" valign="top" width="34.089999999999996%" headers="mcps1.2.4.1.2 "><p id="p10381124244117"><a name="p10381124244117"></a><a name="p10381124244117"></a>mount fileSystemType src dst flags data</p>
<p id="p572019493485"><a name="p572019493485"></a><a name="p572019493485"></a>如：mount vfat /dev/mmcblk0 /sdc rw,umask=000</p>
<p id="p7381173625313"><a name="p7381173625313"></a><a name="p7381173625313"></a>mount jffs2 /dev/mtdblock3 /storage nosuid</p>
</td>
<td class="cellrowborder" valign="top" width="55.76%" headers="mcps1.2.4.1.3 "><p id="p11976107144710"><a name="p11976107144710"></a><a name="p11976107144710"></a>挂载命令，各参数之间有且仅有一个空格。flags当前仅支持nodev、noexec、nosuid、rdonly，data为可选字段。</p>
</td>
</tr>
<tr id="row1334911198482"><td class="cellrowborder" valign="top" width="10.15%" headers="mcps1.2.4.1.1 "><p id="p1214153117480"><a name="p1214153117480"></a><a name="p1214153117480"></a>start</p>
</td>
<td class="cellrowborder" valign="top" width="34.089999999999996%" headers="mcps1.2.4.1.2 "><p id="p133816420411"><a name="p133816420411"></a><a name="p133816420411"></a>start serviceName</p>
<p id="p2036714132541"><a name="p2036714132541"></a><a name="p2036714132541"></a>如：start foundation</p>
<p id="p115951820185412"><a name="p115951820185412"></a><a name="p115951820185412"></a>start shell</p>
</td>
<td class="cellrowborder" valign="top" width="55.76%" headers="mcps1.2.4.1.3 "><p id="p4350121915488"><a name="p4350121915488"></a><a name="p4350121915488"></a>启动服务命令，start后面跟着service名称，该service名称必须能够在services数组中找到。</p>
</td>
</tr>
<tr id="row96921442712"><td class="cellrowborder" valign="top" width="10.15%" headers="mcps1.2.4.1.1 "><p id="p1693642018"><a name="p1693642018"></a><a name="p1693642018"></a>loadcfg</p>
</td>
<td class="cellrowborder" valign="top" width="34.089999999999996%" headers="mcps1.2.4.1.2 "><p id="p1969364211116"><a name="p1969364211116"></a><a name="p1969364211116"></a>loadcfg filePath</p>
<p id="p1858112368211"><a name="p1858112368211"></a><a name="p1858112368211"></a>如：loadcfg /patch/fstab.cfg</p>
</td>
<td class="cellrowborder" valign="top" width="55.76%" headers="mcps1.2.4.1.3 "><p id="p13986141320510"><a name="p13986141320510"></a><a name="p13986141320510"></a>加载其他cfg文件命令。后面跟着的目标文件大小不得超过50KB，且目前仅支持加载/patch/fstab.cfg，其他文件路径和文件名均不支持。/patch/fstab.cfg文件的每一行都是一条命令，命令类型和格式必须符合本表格描述，命令条数不得超过20条。</p>
</td>
</tr>
<tr id="row96921442712"><td class="cellrowborder" valign="top" width="10.15%" headers="mcps1.2.4.1.1 "><p id="p1693642018"><a name="p1693642018"></a><a name="p1693642018"></a>export</p>
</td>
<td class="cellrowborder" valign="top" width="34.089999999999996%" headers="mcps1.2.4.1.2 "><p id="p1969364211116"><a name="p1969364211116"></a><a name="p1969364211116"></a>export key value</p>
<p id="p1858112368211"><a name="p1858112368211"></a><a name="p1858112368211"></a>如：export TEST /data/test</p>
</td>
<td class="cellrowborder" valign="top" width="55.76%" headers="mcps1.2.4.1.3 "><p id="p13986141320510"><a name="p13986141320510"></a><a name="p13986141320510"></a>设置环境变量命令。后面跟两个参数，第一个参数是环境变量名，第二个参数是环境变量值。</p>
</td>
</tr>
<tr id="row96921442712"><td class="cellrowborder" valign="top" width="10.15%" headers="mcps1.2.4.1.1 "><p id="p1693642018"><a name="p1693642018"></a><a name="p1693642018"></a>rm</p>
</td>
<td class="cellrowborder" valign="top" width="34.089999999999996%" headers="mcps1.2.4.1.2 "><p id="p1969364211116"><a name="p1969364211116"></a><a name="p1969364211116"></a>rm filename</p>
<p id="p1858112368211"><a name="p1858112368211"></a><a name="p1858112368211"></a>如：rm /data/testfile</p>
</td>
<td class="cellrowborder" valign="top" width="55.76%" headers="mcps1.2.4.1.3 "><p id="p13986141320510"><a name="p13986141320510"></a><a name="p13986141320510"></a>删除文件命令。后面跟一个参数，即文件的绝对路径。</p>
</td>
</tr>
<tr id="row96921442712"><td class="cellrowborder" valign="top" width="10.15%" headers="mcps1.2.4.1.1 "><p id="p1693642018"><a name="p1693642018"></a><a name="p1693642018"></a>rmdir</p>
</td>
<td class="cellrowborder" valign="top" width="34.089999999999996%" headers="mcps1.2.4.1.2 "><p id="p1969364211116"><a name="p1969364211116"></a><a name="p1969364211116"></a>rmdir dirname</p>
<p id="p1858112368211"><a name="p1858112368211"></a><a name="p1858112368211"></a>如：rmdir /data/testdir</p>
</td>
<td class="cellrowborder" valign="top" width="55.76%" headers="mcps1.2.4.1.3 "><p id="p13986141320510"><a name="p13986141320510"></a><a name="p13986141320510"></a>删除目录命令。后面跟一个参数，即目录的绝对路径。</p>
</td>
</tr>
<tr id="row96921442712"><td class="cellrowborder" valign="top" width="10.15%" headers="mcps1.2.4.1.1 "><p id="p1693642018"><a name="p1693642018"></a><a name="p1693642018"></a>write</p>
</td>
<td class="cellrowborder" valign="top" width="34.089999999999996%" headers="mcps1.2.4.1.2 "><p id="p1969364211116"><a name="p1969364211116"></a><a name="p1969364211116"></a>write filename value</p>
<p id="p1858112368211"><a name="p1858112368211"></a><a name="p1858112368211"></a>如：write /data/testfile 0</p>
</td>
<td class="cellrowborder" valign="top" width="55.76%" headers="mcps1.2.4.1.3 "><p id="p13986141320510"><a name="p13986141320510"></a><a name="p13986141320510"></a>写文件命令。后面跟两个参数，第一个参数是文件的绝对路径，第二个参数是要写入文件的字符串。</p>
</td>
</tr>
<tr id="row96921442712"><td class="cellrowborder" valign="top" width="10.15%" headers="mcps1.2.4.1.1 "><p id="p1693642018"><a name="p1693642018"></a><a name="p1693642018"></a>stop</p>
</td>
<td class="cellrowborder" valign="top" width="34.089999999999996%" headers="mcps1.2.4.1.2 "><p id="p1969364211116"><a name="p1969364211116"></a><a name="p1969364211116"></a>stop servicename</p>
<p id="p1858112368211"><a name="p1858112368211"></a><a name="p1858112368211"></a>如：stop console</p>
</td>
<td class="cellrowborder" valign="top" width="55.76%" headers="mcps1.2.4.1.3 "><p id="p13986141320510"><a name="p13986141320510"></a><a name="p13986141320510"></a>关闭服务命令。后面跟一个参数，即要关闭的服务名。</p>
</td>
</tr>
<tr id="row96921442712"><td class="cellrowborder" valign="top" width="10.15%" headers="mcps1.2.4.1.1 "><p id="p1693642018"><a name="p1693642018"></a><a name="p1693642018"></a>copy</p>
</td>
<td class="cellrowborder" valign="top" width="34.089999999999996%" headers="mcps1.2.4.1.2 "><p id="p1969364211116"><a name="p1969364211116"></a><a name="p1969364211116"></a>copy oldfile newfile</p>
<p id="p1858112368211"><a name="p1858112368211"></a><a name="p1858112368211"></a>如：copy /data/old /data/new</p>
</td>
<td class="cellrowborder" valign="top" width="55.76%" headers="mcps1.2.4.1.3 "><p id="p13986141320510"><a name="p13986141320510"></a><a name="p13986141320510"></a>拷贝文件命令。后面跟两个参数，第一个参数是原文件绝对路径，第二个参数是新文件绝对路径。</p>
</td>
</tr>
<tr id="row96921442712"><td class="cellrowborder" valign="top" width="10.15%" headers="mcps1.2.4.1.1 "><p id="p1693642018"><a name="p1693642018"></a><a name="p1693642018"></a>reset</p>
</td>
<td class="cellrowborder" valign="top" width="34.089999999999996%" headers="mcps1.2.4.1.2 "><p id="p1969364211116"><a name="p1969364211116"></a><a name="p1969364211116"></a>reset servicename</p>
<p id="p1858112368211"><a name="p1858112368211"></a><a name="p1858112368211"></a>如：reset console</p>
</td>
<td class="cellrowborder" valign="top" width="55.76%" headers="mcps1.2.4.1.3 "><p id="p13986141320510"><a name="p13986141320510"></a><a name="p13986141320510"></a>重启服务命令。后面跟一个参数，即要重启的服务名。目前reset命令的策略是，如果一个服务没有启动，则该命令会将其拉起，如果一个服务处于运行状态，则该命令会将其关闭后重启。</p>
</td>
</tr>
<tr id="row96921442712"><td class="cellrowborder" valign="top" width="10.15%" headers="mcps1.2.4.1.1 "><p id="p1693642018"><a name="p1693642018"></a><a name="p1693642018"></a>reboot</p>
</td>
<td class="cellrowborder" valign="top" width="34.089999999999996%" headers="mcps1.2.4.1.2 "><p id="p1969364211116"><a name="p1969364211116"></a><a name="p1969364211116"></a>reboot (subsystem)</p>
<p id="p1858112368211"><a name="p1858112368211"></a><a name="p1858112368211"></a>如：reboot updater</p>
</td>
<td class="cellrowborder" valign="top" width="55.76%" headers="mcps1.2.4.1.3 "><p id="p13986141320510"><a name="p13986141320510"></a><a name="p13986141320510"></a>重启系统命令。后面可以跟一个参数，也可以没有参数，当没有参数时执行该命令，将会使设备重启到当前系统，当后面跟参数时，参数应当是子系统的名字，例如,reboot updater，将会重启进入updater子系统。</p>
</td>
</tr>
<tr id="row96921442712"><td class="cellrowborder" valign="top" width="10.15%" headers="mcps1.2.4.1.1 "><p id="p1693642018"><a name="p1693642018"></a><a name="p1693642018"></a>sleep</p>
</td>
<td class="cellrowborder" valign="top" width="34.089999999999996%" headers="mcps1.2.4.1.2 "><p id="p1969364211116"><a name="p1969364211116"></a><a name="p1969364211116"></a>sleep time</p>
<p id="p1858112368211"><a name="p1858112368211"></a><a name="p1858112368211"></a>如：sleep 5</p>
</td>
<td class="cellrowborder" valign="top" width="55.76%" headers="mcps1.2.4.1.3 "><p id="p13986141320510"><a name="p13986141320510"></a><a name="p13986141320510"></a>睡眠命令。后面可以跟一个参数，该参数是睡眠时间。</p>
</td>
</tr>
</tbody>
</table>

**表 3**  service字段说明

<a name="table14737791471"></a>
<table><thead align="left"><tr id="row273839577"><th class="cellrowborder" valign="top" width="10.37%" id="mcps1.2.3.1.1"><p id="p107382095711"><a name="p107382095711"></a><a name="p107382095711"></a>字段名</p>
</th>
<th class="cellrowborder" valign="top" width="89.63%" id="mcps1.2.3.1.2"><p id="p17738189277"><a name="p17738189277"></a><a name="p17738189277"></a>说明</p>
</th>
</tr>
</thead>
<tbody><tr id="row17386911716"><td class="cellrowborder" valign="top" width="10.37%" headers="mcps1.2.3.1.1 "><p id="p17384912710"><a name="p17384912710"></a><a name="p17384912710"></a>name</p>
</td>
<td class="cellrowborder" valign="top" width="89.63%" headers="mcps1.2.3.1.2 "><p id="p1173818913714"><a name="p1173818913714"></a><a name="p1173818913714"></a>当前服务的名称，须确保非空且长度≤32字节。</p>
</td>
</tr>
<tr id="row1473810916714"><td class="cellrowborder" valign="top" width="10.37%" headers="mcps1.2.3.1.1 "><p id="p127381991571"><a name="p127381991571"></a><a name="p127381991571"></a>path</p>
</td>
<td class="cellrowborder" valign="top" width="89.63%" headers="mcps1.2.3.1.2 "><p id="p1073815910717"><a name="p1073815910717"></a><a name="p1073815910717"></a>当前服务的可执行文件全路径和参数，数组形式。须确保第一个数组元素为可执行文件路径、数组元素个数≤20、每个元素为字符串形式以及每个字符串长度≤64字节。</p>
</td>
</tr>
<tr id="row77381291271"><td class="cellrowborder" valign="top" width="10.37%" headers="mcps1.2.3.1.1 "><p id="p77381391770"><a name="p77381391770"></a><a name="p77381391770"></a>uid</p>
</td>
<td class="cellrowborder" valign="top" width="89.63%" headers="mcps1.2.3.1.2 "><p id="p107387920711"><a name="p107387920711"></a><a name="p107387920711"></a>当前服务进程的uid值。</p>
</td>
</tr>
<tr id="row127381591673"><td class="cellrowborder" valign="top" width="10.37%" headers="mcps1.2.3.1.1 "><p id="p47388919715"><a name="p47388919715"></a><a name="p47388919715"></a>gid</p>
</td>
<td class="cellrowborder" valign="top" width="89.63%" headers="mcps1.2.3.1.2 "><p id="p12738691479"><a name="p12738691479"></a><a name="p12738691479"></a>当前服务进程的gid值。</p>
</td>
</tr>
<tr id="row127381591693"><td class="cellrowborder" valign="top" width="10.37%" headers="mcps1.2.3.1.1 "><p id="p47388919793"><a name="p47388919793"></a><a name="p47388919715"></a>secon</p>
</td>
<td class="cellrowborder" valign="top" width="89.63%" headers="mcps1.2.3.1.2 "><p id="p12738691493"><a name="p12738691493"></a><a name="p12738691493"></a>当前服务进程的安全上下文（当前不需要设置该字段）。</p>
</td>
</tr>
<tr id="row188301014171116"><td class="cellrowborder" valign="top" width="10.37%" headers="mcps1.2.3.1.1 "><p id="p183112146115"><a name="p183112146115"></a><a name="p183112146115"></a>once</p>
</td>
<td class="cellrowborder" valign="top" width="89.63%" headers="mcps1.2.3.1.2 "><p id="p18548317195715"><a name="p18548317195715"></a><a name="p18548317195715"></a>当前服务进程是否为一次性进程：</p>
<p id="p103571840105812"><a name="p103571840105812"></a><a name="p103571840105812"></a>1：一次性进程，当该进程退出时，init不会重新启动该服务进程</p>
<p id="p5831191431116"><a name="p5831191431116"></a><a name="p5831191431116"></a>0 : 常驻进程，当该进程退出时，init收到SIGCHLD信号并重新启动该服务进程；</p>
<p id="p378912714010"><a name="p378912714010"></a><a name="p378912714010"></a>注意：对于常驻进程，若在4分钟之内连续退出5次，第5次退出时init将不会再重新拉起该服务进程。</p>
</td>
</tr>
<tr id="row386110321155"><td class="cellrowborder" valign="top" width="10.37%" headers="mcps1.2.3.1.1 "><p id="p14861113212156"><a name="p14861113212156"></a><a name="p14861113212156"></a>importance</p>
</td>
<td class="cellrowborder" valign="top" width="89.63%" headers="mcps1.2.3.1.2 "><p id="p166448210816"><a name="p166448210816"></a><a name="p166448210816"></a>当前服务进程优先级， 取值范围[19, -20]</p>
</td>
</tr>
<tr id="row1689310618179"><td class="cellrowborder" valign="top" width="10.37%" headers="mcps1.2.3.1.1 "><p id="p108931367177"><a name="p108931367177"></a><a name="p108931367177"></a>caps</p>
</td>
<td class="cellrowborder" valign="top" width="89.63%" headers="mcps1.2.3.1.2 "><p id="p489313618173"><a name="p489313618173"></a><a name="p489313618173"></a>当前服务所需的capability值，根据安全子系统已支持的capability，评估所需的capability，遵循最小权限原则配置（当前最多可配置100个值）。</p>
</td>
</tr>
<tr id="row1689310618179"><td class="cellrowborder" valign="top" width="10.37%" headers="mcps1.2.3.1.1 "><p id="p108931367177"><a name="p108931367177"></a><a name="p108931367177"></a>critical</p>
</td>
<td class="cellrowborder" valign="top" width="89.63%" headers="mcps1.2.3.1.2 "><p id="p489313618173"><a name="p489313618173"></a><a name="p489313618173"></a>critical服务启动失败后， 需要M秒内重新拉起， 拉起失败N次后， 直接重启系统， N默认为4， M默认20。（仅L2以上提供 "critical" : [0, 2, 10]）</p>
<p id="p103571840105812"><a name="p103571840105812"></a><a name="p103571840105812"></a>0：不使能；</p>
<p id="p103571840105812"><a name="p103571840105812"></a><a name="p103571840105812"></a>1：使能；</p>
</td>
</tr>
<tr id="row1689310618179"><td class="cellrowborder" valign="top" width="10.37%" headers="mcps1.2.3.1.1 "><p id="p108931367177"><a name="p108931367177"></a><a name="p108931367177"></a>cpucores</p>
</td>
<td class="cellrowborder" valign="top" width="89.63%" headers="mcps1.2.3.1.2 "><p id="p489313618173"><a name="p489313618173"></a><a name="p489313618173"></a>服务需要的绑定的cpu核心数， 类型为int型数组</p>
</td>
</tr>
<tr id="row1689310618179"><td class="cellrowborder" valign="top" width="10.37%" headers="mcps1.2.3.1.1 "><p id="p108931367177"><a name="p108931367177"></a><a name="p108931367177"></a>d-caps</p>
</td>
<td class="cellrowborder" valign="top" width="89.63%" headers="mcps1.2.3.1.2 "><p id="p489313618173"><a name="p489313618173"></a><a name="p489313618173"></a>分布式能力 (仅L2以上提供)</p>
</td>
</tr>
<tr id="row1689310618179"><td class="cellrowborder" valign="top" width="10.37%" headers="mcps1.2.3.1.1 "><p id="p108931367177"><a name="p108931367177"></a><a name="p108931367177"></a>apl</p>
</td>
<td class="cellrowborder" valign="top" width="89.63%" headers="mcps1.2.3.1.2 "><p id="p489313618173"><a name="p489313618173"></a><a name="p489313618173"></a>能力特权级别：system_core, normal, system_basic。 默认system_core (仅L2以上提供)</p>
</td>
</tr>
<tr id="row1689310618179"><td class="cellrowborder" valign="top" width="10.37%" headers="mcps1.2.3.1.1 "><p id="p108931367177"><a name="p108931367177"></a><a name="p108931367177"></a>start-mode</p>
</td>
<td class="cellrowborder" valign="top" width="89.63%" headers="mcps1.2.3.1.2 "><p id="p489313618173"><a name="p489313618173"></a><a name="p489313618173"></a>服务的启动模式，具体描述：init服务启动控制(仅L2以上提供)</p>
</td>
</tr>
<tr id="row1689310618179"><td class="cellrowborder" valign="top" width="10.37%" headers="mcps1.2.3.1.1 "><p id="p108931367177"><a name="p108931367177"></a><a name="p108931367177"></a>jobs</p>
</td>
<td class="cellrowborder" valign="top" width="89.63%" headers="mcps1.2.3.1.2 "><p id="p489313618173"><a name="p489313618173"></a><a name="p489313618173"></a>当前服务在不同阶段可以执行的job。具体说明可以看：init服务并行控制(仅L2以上提供)</p>
</td>
</tr>
<tr id="row1689310618179"><td class="cellrowborder" valign="top" width="10.37%" headers="mcps1.2.3.1.1 "><p id="p108931367177"><a name="p108931367177"></a><a name="p108931367177"></a>ondemand</p>
</td>
<td class="cellrowborder" valign="top" width="89.63%" headers="mcps1.2.3.1.2 "><p id="p489313618173"><a name="p489313618173"></a><a name="p489313618173"></a>按需启动的服务需要配置的属性，配置有该属性的服务不会被start命令拉起，如果该服务同时配置有socket，init将会在解析服务时创建该socket并将其监听，当socket有消息上报时，init拉起对应服务</p>
</td>
</tr>
</tbody>
</table>

## 相关仓<a name="section641143415335"></a>

[启动恢复子系统](https://gitee.com/openharmony/docs/blob/master/zh-cn/readme/%E5%90%AF%E5%8A%A8%E6%81%A2%E5%A4%8D%E5%AD%90%E7%B3%BB%E7%BB%9F.md)

[startup\_syspara\_lite](https://gitee.com/openharmony/startup_syspara_lite/blob/master/README_zh.md)

[startup\_appspawn\_lite](https://gitee.com/openharmony/startup_appspawn_lite/blob/master/README_zh.md)

[startup\_bootstrap\_lite](https://gitee.com/openharmony/startup_bootstrap_lite/blob/master/README_zh.md)

**[startup\_init\_lite]**

