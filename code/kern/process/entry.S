.text
.globl kernel_thread_entry
kernel_thread_entry:        # void kernel_thread(void)
    # 例如创建 kernel_thread(init_main, 字符串, 0)，恢复后 s0=init_main，s1=字符串地址。
    # move 完成后 a0 就是 init_main 的参数；jalr 同时把函数返回点记录到 ra。
    # 中文导读：初始现场的 s0 是线程函数，s1 是参数，先把参数移到 ABI 指定的 a0。
	move a0, s1
	jalr s0

    # 线程返回值保留在 a0，直接传给退出入口；当前 do_exit 进入 panic。
	jal do_exit
