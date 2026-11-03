# 模块一最终提示词
```
[PROMPT]

**任务**：
完成 lab3 的“Trap 上下文恢复与 Trap 入口功能”。
目标文件为 kern/trap/trapentry.S，仅补齐 RESTORE_ALL 和
__alltraps 中标有“lab3 function1 YOUR CODE”的位置。

**操作要求**：
保留现有代码和实验标记，不修改 SAVE_ALL、__trapret 的既有返回逻辑、
struct trapframe、trap() 签名及模块二代码。

**输出要求**：
分别给出 RESTORE_ALL 和 __alltraps 的完整实现，
解释寄存器恢复顺序、参数传递和 Trap 返回机制。
验证结果必须区分汇编编译、独立上下文测试和完整 lab3 评分。

[RELY]

- 目标平台为 RV64，REGBYTES 为 8；使用工程提供的 LOAD、STORE 宏。
- SAVE_ALL 已在当前内核栈上建立 Trap frame：
  - 第 0～31 槽依次对应 x0～x31；
  - 第 32 槽为 status，对应 sstatus；
  - 第 33 槽为 epc，对应 sepc；
  - 第 34 槽为 badvaddr；
  - 第 35 槽为 cause。
- 每个槽占 REGBYTES 字节。
- SAVE_ALL 完成后，sp 指向 Trap frame 起始地址。
- 第 2 槽保存的是分配 Trap frame 之前的原始 sp，
  由 SAVE_ALL 经 sscratch 间接取得；此时 sscratch 已清零。
- 框架提供接口：
  void trap(struct trapframe *tf);
- RISC-V 调用约定使用 a0 传递第一个参数，
  C 函数正常返回时应恢复调用前的 sp。
- __trapret 已包含 RESTORE_ALL 和 sret。
- sret 根据 sepc 返回，并依据 sstatus 中的 SPP、SPIE 等字段
  完成特权级和中断使能状态的转换。

[GUARANTEE]

必须补齐：
1. 汇编宏 RESTORE_ALL。
2. 汇编入口 __alltraps 中 SAVE_ALL 之后的 C 函数调用部分。

不新增公开接口、辅助函数、全局变量或其他模块功能。

[SPECIFICATION]

## RESTORE_ALL

**Pre-Condition**：
- sp 指向合法、可读的 Trap frame，其布局与 SAVE_ALL 一致。
- C 语言处理已经完成，frame 中的返回状态可能已被合法修改。
- 当前处于可以执行相关 CSR 操作的 S 模式。

**Post-Condition**：
- x1～x31 恢复为 frame 中对应的值，x0 保持恒为零。
- sstatus 和 sepc 恢复为 frame 中的 status 和 epc。
- sp 恢复为第 2 槽保存的原始值。
- 后续 sret 能根据恢复后的状态返回。

**Requirements**：
- 恢复其他寄存器期间，sp 必须继续指向当前 frame，
  因此最后恢复 x2；恢复后不得重复增加帧大小。
- 用于恢复 CSR 的临时寄存器，随后也必须恢复其原值。
- 不强制改变 SPP，不提前主动开启中断，不自行增加 sepc。
- badvaddr、cause 仅作为现场信息，不要求写回对应 CSR；
  不将原始 sp 写回 sscratch。

## __alltraps

**Pre-Condition**：
- 处理器已进入 Trap 入口，框架的 SAVE_ALL 可以正常执行。

**Post-Condition**：
- SAVE_ALL 建立的 frame 地址作为第一个参数传入 trap()。
- trap() 正常返回后，执行流进入既有 __trapret，
  完成上下文恢复并执行 sret。

**Requirements**：
- 遵循 RISC-V 调用约定，正确设置第一个参数和函数返回地址。
- 调用前后保持 frame 可供 RESTORE_ALL 使用。
- RV64 下 frame 大小为 36×8＝288 字节；
  在入口栈原本满足 16 字节对齐时，调用 C 函数仍保持对齐。
- 不修改 SAVE_ALL，也不实现模块二的异常或中断处理逻辑。
```

# 模块二最终提示词
```
[PROMPT]

**任务**：
完成 lab3 的“中断与异常处理功能”。
目标文件为 kern/trap/trap.c，仅补齐 interrupt_handler 和
exception_handler，替换标有“lab3 function2 YOUR CODE”的位置。

**操作要求**：
保留现有代码和实验标记，不修改 trap()、trap_dispatch()、
print_trapframe()、struct trapframe、trapentry.S 及模块一代码。

**输出要求**：
给出两个函数的完整实现，说明 cause 的判断方式、时钟重装、
ticks 统计、关机条件、epc 调整依据；
给出 Test 1/2/3 的验证方法。

[RELY]

- trapframe 见 kern/trap/trap.h：gpr、status、epc、badvaddr、cause。
- scause 最高位（bit 63）为 1 表示中断，其余位为原因编号。
- 宏定义：IRQ_S_SOFT=1、IRQ_S_TIMER=5、IRQ_S_EXT=9、
  CAUSE_ILLEGAL_INSTRUCTION=2、CAUSE_BREAKPOINT=3。
- TICK_NUM=100；ticks 是 clock.h 中的 volatile size_t。
- clock_set_next_event() 用于设置下一次时钟事件；
  print_ticks() 已实现；sbi_shutdown() 在 libs/sbi.h 中。
- 异常返回地址由 tf->epc 决定，返回前由模块一恢复 sepc。
- 指令可能是 32 位或 16 位压缩指令，低两位为 0b11 表示 32 位。
- SHUTDOWN_AFTER 实验框架未定义，需要在 trap.c 中补充。
- 未列出的中断或异常只要求用 print_trapframe(tf) 兜底。

[GUARANTEE]

必须实现：
1. void interrupt_handler(struct trapframe *tf);
2. void exception_handler(struct trapframe *tf);
不修改函数签名，不新增公开接口或其他模块功能。

[SPECIFICATION]

## interrupt_handler

**Pre-Condition**：
- tf 指向合法的 trapframe。
- 通过 trap_dispatch 调用时 cause 最高位为 1；
  测试代码也可能直接传入正数 cause。

**Post-Condition**：
- IRQ_S_TIMER：重装下一次时钟事件，ticks 加一；
  每 TICK_NUM 次输出一次统计；达到 SHUTDOWN_AFTER 次统计后关机。
- IRQ_S_SOFT：输出“Supervisor software interrupt”。
- IRQ_S_EXT：输出“Supervisor external interrupt”。
- 其他：输出“Unknown interrupt: cause N”，并调用 print_trapframe(tf)。

**Requirements**：
- 判断前先用 cause &= ~(1UL << 63) 去掉中断标志位，
  不能只依赖有符号数判断。
- 时钟中断必须重装定时器，否则系统只能收到一次时钟中断。
- ticks 每收到一次时钟中断加一，统计输出不得改变递增规则。
- 关机只在第 SHUTDOWN_AFTER 次统计输出后执行。

## exception_handler

**Pre-Condition**：
- tf 有效，cause 为异常编号，最高位为 0。

**Post-Condition**：
- CAUSE_ILLEGAL_INSTRUCTION：输出“Illegal instruction at 0x...”，
  并把 epc 前进一条指令的长度。
- CAUSE_BREAKPOINT：输出“Breakpoint at 0x...”，
  并把 epc 前进一条指令的长度。
- 其他：调用 print_trapframe(tf)。

**Requirements**：
- 读取 epc 处指令的最低两位：0b11 为 4 字节，否则为 2 字节。
- 不得统一按 4 字节推进，否则遇到压缩指令会跳到错误位置。
- 输出格式必须与实验评测检查的字符串一致。
```