/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *      The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2014 Serge Vakulenko
 * Copyright (c) 2015 Ivaylo Krastev
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: clock.c 1.18 91/01/21$
 *
 *      @(#)clock.c     8.2 (Berkeley) 10/9/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/cpu.h>
#include <machine/machConst.h>
#include <machine/pic32mz.h>

#define SECMIN  60U                     /* seconds per minute */
#define SECHOUR (60U * SECMIN)          /* seconds per hour */
#define SECDAY  (24U * SECHOUR)         /* seconds per day */
#define SECYR   (365U * SECDAY)         /* seconds per common year */

#define LEAPYEAR(year)  (((year) % 4) == 0)

/*
 * Write sequence unlock keys
 */
#define UNLOCK_KEY1 0xaa996655;
#define UNLOCK_KEY2 0x556699aa;

/*
 * BCD to decimal and decimal to BCD.
 */
#define FROMBCD(x)  (((x) >> 4) * 10 + ((x) & 0xf))
#define TOBCD(x)    (((x) / 10 * 16) + ((x) % 10))

/*
 * This is the amount to add to the value stored in the clock chip
 * to get the current year.
 */
#define YR_OFFSET       2000

/*
 * This code is defunct after 2099.
 * Will Unix still be here then??
 */
static short dayyr[12] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

struct chiptime {
    int sec;
    int min;
    int hour;
    int wday;
    int day;
    int mon;
    int year;
};

/*
 * Convert RTC chip values to Unix time
 */
time_t
chiptotime(sec, min, hour, day, mon, year)
    int sec, min, hour, day, mon, year;
{
    int days, yr;

    sec = FROMBCD(sec);
    min = FROMBCD(min);
    hour = FROMBCD(hour);
    day = FROMBCD(day);
    mon = FROMBCD(mon);
    year = FROMBCD(year) + YR_OFFSET;

    /* simple sanity checks */
    if (year < 2000 || year > 2099 || mon < 1 || mon > 12 ||
          day < 1 || day > 31 || hour > 23 || min > 59 || sec > 59)
        return 0;

    days = 0;
    for (yr = 1970; yr < year; yr++)
        days += LEAPYEAR(yr) ? 366 : 365;
    days += dayyr[mon - 1] + day - 1;
    if (LEAPYEAR(yr) && mon > 2)
        days++;

    /* now have days since Jan 1, 1970; the rest is easy... */
    return (days * SECDAY + hour * 3600 + min * 60 + sec);
}

/*
 * Convert Unix time to RTC chip values
 */
int
timetochip(c)
    struct chiptime *c;
{
    int t, t2, t3, now = time.tv_sec;

    /* compute the year */
    t2 = now / SECDAY;
    t3 = (t2 + 4) % 7;  /* day of week 0-6, 0 = sunday */
    c->wday = TOBCD(t3);

    t = 1969;
    while (t2 >= 0) {   /* whittle off years */
        t3 = t2;
        t++;
        t2 -= LEAPYEAR(t) ? 366 : 365;
    }
    c->year = t;

    /* t3 = month + day; separate */
    t = LEAPYEAR(t);
    for (t2 = 1; t2 < 12; t2++)
        if (t3 < dayyr[t2] + (t && t2 > 1))
            break;

    /* t2 is month */
    c->mon = t2;
    c->day = t3 - dayyr[t2 - 1] + 1;
    if (t && t2 > 2)
        c->day--;

    /* the rest is easy */
    t = now % SECDAY;
    c->hour = t / 3600;
    t %= 3600;
    c->min = t / 60;
    c->sec = t % 60;

    /* simple sanity checks */
    if (c->year < 2000 || c->year > 2099 || c->mon < 1 || c->mon > 12 ||
        c->day < 1 || c->day > 31 || c->hour > 23 || c->min > 59 || c->sec > 59)
        return 0;

    c->sec = TOBCD(c->sec);
    c->min = TOBCD(c->min);
    c->hour = TOBCD(c->hour);
    c->day = TOBCD(c->day);
    c->mon = TOBCD(c->mon);
    c->year = TOBCD(c->year - YR_OFFSET);
    return 1;
}

/*
 * Enable/Disable RTC write operations (RTCWREN)
 */
void
rtc_wren(onoff)
    int onoff;
{
    int s;
    if (onoff) {
       s = splhigh();
       SYSKEY = 0;
       SYSKEY = UNLOCK_KEY1;
       SYSKEY = UNLOCK_KEY2;
       RTCCONSET = PIC32_RTCC_WREN;
       splx(s);
    } else
       RTCCONCLR = PIC32_RTCC_WREN;
}

/*
 * Start the real-time and statistics clocks. Leave stathz 0 since there
 * are no other timers available.
 */
void
cpu_initclocks()
{
    extern int tickadj;

    hz = HZ;                    /* 100 Hz */
    tick = 1000000 / HZ;        /* number of micro-seconds between interrupts */
    tickadj = 240000 / (60 * HZ);

    unsigned count = mfc0_Count();
    count += (CPU_KHZ * 1000 / HZ + 1) / 2;
    mtc0_Compare (count);
    IECSET(0) = 1 << PIC32_IRQ_CT;
}

/*
 * We assume newhz is either stathz or profhz, and that neither will
 * change after being set up above.  Could recalculate intervals here
 * but that would be a drag.
 */
void
setstatclockrate(newhz)
    int newhz;
{
}

/*
 * Set up the system's time, given a `reasonable' time value.
 */
void
inittodr(base)
    time_t base;
{
    int sec, min, hour, day, mon, year;
    int s, badbase = 0;
    unsigned t1, t2;

    if (base < 5 * SECYR) {
        printf("WARNING: preposterous time in file system\n");
        /* not going to use it anyway, if the chip is readable */
        base = 21*SECYR + 186*SECDAY + SECDAY/2;
        badbase = 1;
    }

    /* get time/date values from RTC chip */
    s = splhigh();
    while (RTCCON & PIC32_RTCC_SYNC);
    t1 = RTCTIME;
    t2 = RTCDATE;
    splx(s);

    sec = t1 >> PIC32_RTCTIME_SEC & PIC32_RTCTIME_SEC_MASK;
    min = t1 >> PIC32_RTCTIME_MIN & PIC32_RTCTIME_MIN_MASK;
    hour = t1 >> PIC32_RTCTIME_HOUR & PIC32_RTCTIME_HOUR_MASK;
    day = t2 >> PIC32_RTCDATE_DAY & PIC32_RTCDATE_DAY_MASK;
    mon = t2 >> PIC32_RTCDATE_MONTH & PIC32_RTCDATE_MONTH_MASK;
    year = t2 >> PIC32_RTCDATE_YEAR & PIC32_RTCDATE_YEAR_MASK;

    if ((time.tv_sec = chiptotime(sec, min, hour, day, mon, year)) == 0) {
        printf("WARNING: bad date in battery clock");
        /*
         * Believe the time in the file system for lack of
         * anything better, resetting the clock.
         */
        time.tv_sec = base;
        if (!badbase)
            resettodr();
    } else {
        int deltat = time.tv_sec - base;

        if (deltat < 0)
            deltat = -deltat;
        if (deltat < 2 * SECDAY)
            return;
        printf("WARNING: clock %s %d days",
            time.tv_sec < base ? "lost" : "gained", deltat / SECDAY);
    }
    printf(" -- CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the clock based on the current time.
 * Used when the current clock is preposterous, when the time is changed,
 * and when rebooting.  Do nothing if the time is not yet known, e.g.,
 * when crashing during autoconfig.
 */
void
resettodr()
{
    struct chiptime c;
    unsigned t1, t2;

    if (!time.tv_sec)
         return;

    /* save current time/date to RTC chip */
    if (timetochip(&c)) {
       t1 = c.sec << PIC32_RTCTIME_SEC |
            c.min << PIC32_RTCTIME_MIN |
            c.hour << PIC32_RTCTIME_HOUR;

       t2 = c.wday |
            c.day << PIC32_RTCDATE_DAY |
            c.mon << PIC32_RTCDATE_MONTH |
            c.year << PIC32_RTCDATE_YEAR;

       rtc_wren(1);
       RTCCONCLR = PIC32_RTCC_ON;
       while (RTCCON & PIC32_RTCC_CLKON);
       RTCTIME = t1;
       RTCDATE = t2;
       RTCCONSET = PIC32_RTCC_ON;
       while (!(RTCCON & PIC32_RTCC_CLKON));
       rtc_wren(0);
    }
}
