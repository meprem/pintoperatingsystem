#ifndef DEVICES_TIMER_H
#define DEVICES_TIMER_H

#include <round.h>
#include <stdint.h>

/**********Linux Kernel Development Second Edition ** By Robert Love*************************************************************************************

The Tick Rate: HZ

The frequency of the system timer (the tick rate) is programmed on system boot based on a static preprocessor define, HZ. The value of HZ differs for each supported architecture. In fact, on some supported architectures, it even differs between machine types.

The kernel defines the value in <asm/param.h>. The tick rate has a frequency of HZ hertz and a period of 1/HZ seconds. For example, in include/asm-i386/param.h, the i386 architecture defines:

#define HZ 1000        // internal kernel time frequency


Therefore, the timer interrupt on i386 has a frequency of 1000HZ and occurs 1,000 times per second (every one-thousandth of a second, which is every millisecond). Most other architectures have a tick rate of 100. Table 10.1 is a complete listing of the supported architectures and their defined tick rates.

Table 10.1. Frequency of the Timer Interrupt
Architecture
Frequency (in Hertz)
Alpha
1024
Arm
100
Cris
100
h8300
100
i386
1000
ia64
32 or 1024[2]
m68k
100
m68knommu
50, 100, or 1000
Mips
100
mips64
100 or 1000
Parisc
100 or 1000
Ppc
1000
ppc64
1000
s390
100
Sh
100 or 1000
Sparc
100
sparc64
1000
Um
100
v850
24, 100, or 122
x86-64
1000
    [2] The IA-64 simulator has a tick rate of 32Hz. Real IA-64 machines have a tick rate of 1024Hz.

When writing kernel code, never assume that HZ has any given value. This is not a common mistake these days because so many architectures have varying tick rates. In the past, however, Alpha was the only architecture with a tick rate not equal to 100Hz, and it was common to see code incorrectly hard-code the value 100 when the HZ value should have been used. Examples of using HZ in kernel code are shown later.

The frequency of the timer interrupt is rather important. As you already saw, the timer interrupt performs a lot of work. Indeed, the kernel's entire notion of time derives from the periodicity of the system timer. Picking the right value, like a successful relationship, is all about compromise.

Increasing the tick rate means the timer interrupt runs more frequently. Consequently, the work it performs occurs more often. This has the following benefits:

    *	The timer interrupt has a higher resolution and, consequently, all timed events have a higher resolution
    *	The accuracy of timed events improves

This higher resolution and greater accuracy provides multiple advantages:

    *	Kernel timers execute with finer resolution and increased accuracy (this provides a large number of improvements, one of which is the following).
    *   System calls such as poll() and select() that optionally employ a timeout value execute with improved precision.
    *   Measurements, such as resource usage or the system uptime, are recorded with a finer resolution.
    *   Process preemption occurs more accurately.

Some of the most readily noticeable performance benefits come from the improved precision of poll() and select()  timeouts. The improvement might be quite large; an application that makes heavy use of these system calls might waste a great deal of time waiting for the timer interrupt, when, in fact, the timeout has actually expired. Remember, the average error (that is, potentially wasted time) is half the period of the timer interrupt.

Another benefit of a higher tick rate is the greater accuracy in process preemption, which results in decreased scheduling latency. Recall from Chapter 4 that the timer interrupt is responsible for decrementing the running process's timeslice count. When the count reaches zero, need_resched is set and the kernel runs the scheduler as soon as possible. Now assume a given process is running and has 2 milliseconds of its timeslice remaining. In 2 milliseconds, the scheduler should preempt the running process and begin executing a new process. Unfortunately, this event does not occur until the next timer interrupt, which might not be in 2 milliseconds. In fact, at worst the next timer interrupt might be 1/HZ of a second away! With HZ=100, a process can get nearly ten extra milliseconds to run. Of course, this all balances out and fairness is preserved, because all tasks receive the same imprecision in schedulingbut that is not the issue. The problem stems from the latency created by the delayed preemption. If the to-be-scheduled task had something time sensitive to do, such as refill an audio buffer, the delay might not be acceptable. Increasing the tick rate to 1000Hz lowers the worst-case scheduling overrun to just 1 millisecond, and the average-case overrun to just 0.5 milliseconds.

Now, there must be some downside to increasing the tick rate or it would have been 1000Hz (or even higher) to start. Indeed, there is one large issue: A higher tick rate implies more frequent timer interrupts, which implies higher overhead, because the processor must spend more time executing the timer interrupt handler. The higher the tick rate, the more time the processor spends executing the timer interrupt. This adds up to not just less processor time available for other work, but also a more frequent thrashing of the processor's cache. The issue of the overhead's impact is debatable. A move from HZ=100 to HZ=1000 clearly brings with it ten times greater overhead. However, how substantial is the overhead to begin with? The final agreement is that, at least on modern systems, HZ=1000  does not create unacceptable overhead and the move to a 1000Hz timer has not hurt performance too much. Nevertheless, it is possible in 2.6 to compile the kernel with a different value for HZ[4].

***************************************************************************************************************************************************************/

/* Number of timer interrupts per second. */
#define TIMER_FREQ 100

void timer_init (void);
void timer_calibrate (void);

int64_t timer_ticks (void);
int64_t timer_elapsed (int64_t);

/* Sleep and yield the CPU to other threads. */
void timer_sleep (int64_t ticks);
void timer_msleep (int64_t milliseconds);
void timer_usleep (int64_t microseconds);
void timer_nsleep (int64_t nanoseconds);

/* Busy waits. */
void timer_mdelay (int64_t milliseconds);
void timer_udelay (int64_t microseconds);
void timer_ndelay (int64_t nanoseconds);

void timer_print_stats (void);

#endif /* devices/timer.h */

/*************************************************************************************************************************************************************
Hardware Clocks and Timers

Architectures provide two hardware devices to help with time keeping: the system timer, which we have been discussing, and the real-time clock. The actual behavior and implementation of these devices varies between different machines, but the general purpose and design is about the same for each.
Real-Time Clock

The real-time clock (RTC) provides a nonvolatile device for storing the system time. The RTC continues to keep track of time even when the system is off by way of a small battery typically included on the system board. On the PC architecture, the RTC and the CMOS are integrated and a single battery keeps the RTC running and the BIOS settings preserved.

On boot, the kernel reads the RTC and uses it to initialize the wall time, which is stored in the xtime variable. The kernel does not typically read the value again; however, some supported architectures, such as x86, periodically save the current wall time back to the RTC. Nonetheless, the real time clock's primary importance is only during boot, when the xtime variable is initialized.
System Timer

The system timer serves a much more important (and frequent) role in the kernel's timekeeping. The idea behind the system timer, regardless of architecture, is the sameto provide a mechanism for driving an interrupt at a periodic rate. Some architectures implement this via an electronic clock that oscillates at a programmable frequency. Other systems provide a decrementer: A counter is set to some initial value and decrements at a fixed rate until the counter reaches zero. When the counter reaches zero, an interrupt is triggered. In any case, the effect is the same.

On x86, the primary system timer is the programmable interrupt timer (PIT). The PIT exists on all PC machines and has been driving interrupts since the days of DOS. The kernel programs the PIT on boot to drive the system timer interrupt (interrupt zero) at HZ frequency. It is a simple device with limited functionality, but it gets the job done. Other x86 time sources include the local APIC timer and the processor's time stamp counter (TSC).
*************************************************************************************************************************************************************/
