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