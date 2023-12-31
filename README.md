## 操作系统实验
HITSZ 2023秋 OS实验
### 实验一：Utilities（util）
借助系统调用实现几个简单的命令，主要目的还是熟悉一下xv6，较简单。由于是从xv6实验中摘了几个，我称为小xv6实验。
1. `sleep`：照抄PPT代码
1. `pingpong`：考察管道的使用和进程创建
1. `find`：考察文件相关系统调用的使用，由于接口和Linux不一样，费了点劲儿。但是有`ls.c`作为参考，倒是也还好。
### 实验二：System calls（syscall）
修改和实现几个系统调用，需要花点时间理清从用户空间调用系统接口的全过程。几乎是完全魔改了xv6实验，由于干涉了系统原来的正常运行，不太喜欢。
1. 魔改`exit()`：在进程退出时打印父进程信息和子进程信息，插入几条语句就好
1. 魔改`wait()`：添加一个参数用来指定阻塞与否
1. 实现`yield()`：要求比正常的`yield`多一个信息的打印语句。`proc.c`里原有一个`yield`，一开始直接在上边改的，结果影响到了`kernel`的函数，没能过测。后来旁路了它，实际上还是照抄原来的加了条打印语句。
### 实验三：Lock（lock）
修改原先的`kalloc.c`和`bio.c`优化内存和缓存管理的锁争用问题，比前两个实验难一些，感觉...倒是没怎么帮助理解锁的原理？
1. 优化`kalloc`：要求把统一的内存链表和锁拆分成每CPU一个，要求实现“窃取”功能倒是比较有趣。没遇到什么锁相关的问题。倒是`initlock()`没细看，以为会拷贝传入的字符串，所以传入的参数都放在了栈上，引起了奇怪的问题，研究了一会儿。
1. 优化`bcache`：要求把统一的缓存链表和锁拆分进多个哈希桶，基本上跟`kalloc`的优化没什么区别。但因为缓存分配后也是留在队列里的，“窃取”过程中写得不谨慎可能会同时持有两把锁，这样会有死锁问题。（后面补充：我的代码有竞争问题没处理掉，实际上 `bcache` 的窃取在返回时的逻辑没那么trivial，因为可能会被别的cpu抢先放一个块进来，所以需要再次拿锁后重新检查一遍。虽然，这个问题由于一些原因触发概率其实很小，一般可以过grade，笑）
### 实验四：Page tables（pgtbl）
不得不说，这个实验很难，个人感觉难度超过了前面三个实验加起来。想要真正理解实验要做什么，指导书里每一步为什么要这样做，为什么不是那样做，需要很多的背景知识，比如内核页表和用户态页表来回切换的时机、切换的方式以及具体的实现位置等等。这过程中需要了解各种中断和陷阱的机制，其中不乏有涉及到机器态的定时器中断等等，总之是非常抽象。最难受的一点是，一出问题就跳转kernelvec，这是通过汇编代码直接完成的，不能通过`backtrace`溯源，遇到竞争条件的问题更是主打一个难以复现...<br>
实验有3个任务：
1. 实现内核函数`vmprint()`：打印出当前进程的虚拟内存布局，包括页表和物理页的映射关系。这个任务比较简单，主要是熟悉一下页表的结构，以及页表的遍历方法。
2. 分离内核页表：为每个进程创建一个单独的内核页表，旨在简化`copyin`等函数工作的过程，同时优化性能。只用做到分离页表即可，用户态内存不需要映射到内核页表中。
3. 完成内核页表：把用户态的内存也映射到内核态页表中，然后修改`copyin`和`copyinstr`调用直接访存的版本，从而实现内核页表的使用。
### 实验五：简易文件系统
emmm，模板代码有些地方写得蛮抽象，再加上我是一个晚上通宵写的，虽然过了测试但实际上似乎目录信息没有写回到磁盘（<br>
代码基本从模板代码摸过来改改改，自己觉得太丑陋，外加不基于xv6，就不传上来了。
### Lazy Allocation
听闻xv6 2020版的这个实验有些难度，所以摸过来做做看，由于这个仓库刚好基于2020版，所以就在这里单开一个分支来做这个实验。<br>
实验要求延后 `sbrk` 的内存分配到触发 `usertrap` 时才进行。没想到整个实验反而非常轻松，2个多小时就完成了，简单程度仅次于 `networking` 。<br>
按指导书把 `sys_sbrk` 和 `usertrap` 改一改，再把会 `panic` 的地方全部跳过，然后注意patch一下 `copyin` 、 `copyinstr` 、 `copyout` 这几个函数防止 `read` 、 `write` 等函数出错就好了。