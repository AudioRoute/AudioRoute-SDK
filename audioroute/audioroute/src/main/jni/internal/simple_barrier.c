/*
 * Copyright 2013 Google Inc. All Rights Reserved.
 * Copyright 2019 n-Track S.r.l.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "simple_barrier.h"

#include <limits.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <android/log.h>

#define LOCKLOGD(...)

#define ONE_BILLION 1000000000

static void get_relative_deadline(
    const struct timespec *abstime, struct timespec *reltime) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  if (now.tv_sec < abstime->tv_sec ||
      (now.tv_sec == abstime->tv_sec && now.tv_nsec < abstime->tv_nsec)) {
    reltime->tv_sec = abstime->tv_sec - now.tv_sec;
    if (abstime->tv_nsec >= now.tv_nsec) {
      reltime->tv_nsec = abstime->tv_nsec - now.tv_nsec;
    } else {
      --reltime->tv_sec;
      reltime->tv_nsec = (ONE_BILLION + abstime->tv_nsec) - now.tv_nsec;
    }
  } else {
    reltime->tv_sec = 0;
    reltime->tv_nsec = 0;
  }
}

static void futex_wait(simple_barrier_t *p, struct timespec *abstime) {
  if (abstime == NULL) {
    syscall(__NR_futex, p, FUTEX_WAIT, 0, NULL, NULL, 0, 0);
  } else {
    struct timespec reltime;
    get_relative_deadline(abstime, &reltime);
    syscall(__NR_futex, p, FUTEX_WAIT, 0, &reltime, NULL, 0, 0);
    // Note: Passing struct timespec as ktime_t works for now but may need
    // further consideration when we move to 64bit.
  }
}

int sb_wait(simple_barrier_t *p, struct timespec *abstime) {
  switch (__sync_or_and_fetch(p, 0)) {
    case 0:
      futex_wait(p, abstime);
      break;
    case 1:
      return 0;   // Success!
    default:
      return -2;  // Error; the futex has been tampered with.
  }
  switch (__sync_or_and_fetch(p, 0)) {
    case 0:
      return -1;  // Failure; __futex_wait probably timed out.
    case 1:
      return 0;
    default:
      return -2;
  }
}

int sb_wait_and_clear(simple_barrier_t *p, struct timespec *abstime) {
  switch (__sync_or_and_fetch(p, 0)) {
    case 0:
      futex_wait(p, abstime);
    case 1:
      break;
    default:
      return -2;
  }
  switch (__sync_val_compare_and_swap(p, 1, 0)) {
    case 0:
      return -1;
    case 1:
      return 0;
    default:
      return -2;
  }
}

int sb_wake(simple_barrier_t *p) {
  if (__sync_bool_compare_and_swap(p, 0, 1)) {
    syscall(__NR_futex, p, FUTEX_WAKE, INT_MAX, NULL, NULL, 0, 0);
    return 0;
  } else {
    return -2;
  }
}

void sb_clobber(simple_barrier_t *p) {
  int val = 1;
  while (val = __sync_val_compare_and_swap(p, val, 0));
}

#define LOCKED_VAL mythreadid
#define UNLOCKED_VAL 0

typedef int FutexVal;

static int futex_wait_lock(volatile struct simple_lock_barrier_t *p, struct timespec *abstime, FutexVal val) {
    LOCKLOGD("futex_wait_lock\n");

#ifdef USE_OLD_LOCK
    futex_wait((simple_barrier_t *)&p->barrier, abstime);
    return 0;
#else
    return syscall(__NR_futex, &p->barrier, FUTEX_LOCK_PI, 0, abstime, NULL, 0, 0);
#endif
    // Note: Passing struct timespec as ktime_t works for now but may need
    // further consideration when we move to 64bit.
}

int sb_wait_lock(struct simple_lock_barrier_t *p, struct timespec *abstime) {
#ifdef USE_OLD_LOCK
    sb_wait((simple_barrier_t *)&p->barrier, abstime);
#else
  int mythreadid=syscall(SYS_gettid);
    FutexVal val=__sync_val_compare_and_swap(&p->barrier, UNLOCKED_VAL, LOCKED_VAL);
    if(val==UNLOCKED_VAL) {
        LOCKLOGD("Wait was unlocked %d\n", (int)LOCKED_VAL);
        return 0;
    }
    LOCKLOGD("WAIT: %d\n", val);
    futex_wait_lock(p, abstime, LOCKED_VAL);

  FutexVal  newval=__sync_or_and_fetch(&p->barrier, 0);
    int ret=(newval&(~FUTEX_WAITERS))==LOCKED_VAL ? 0 : -1;
  return ret;
#endif
}

int sb_wait_and_reset_lock(struct simple_lock_barrier_t *p, struct timespec *abstime) {
#ifdef USE_OLD_LOCK
    return sb_wait_and_clear((simple_barrier_t *)&p->barrier, abstime);
#else
  int mythreadid=syscall(SYS_gettid);
  FutexVal val=__sync_val_compare_and_swap(&p->barrier, UNLOCKED_VAL, LOCKED_VAL);
  if(val==UNLOCKED_VAL) {
      LOCKLOGD("Wait was unlocked %d\n", (int)LOCKED_VAL);

      // Reset the lock back to unsignalled from the old owner
      LOCKLOGD("AFTER WAIT: %d restoring %d\n", val, p->runnerThreadId);
      if(__sync_bool_compare_and_swap(&p->barrier, LOCKED_VAL, p->runnerThreadId)) {
          return 0;
      }
      else {
          LOCKLOGD("AFTER WAIT: FAILED restoring lock %d %d\n", val, p->runnerThreadId);
      }
  }
  LOCKLOGD("WAIT: %d mythread: %d\n", val, mythreadid);

  futex_wait_lock(p, abstime, LOCKED_VAL);

  // After the futex wait the futex value should be FUTEX_WAITERS|TID, so we now own the lock and can operate on it non-atomically!?
  // Restore to how it was prior to the wait
  FutexVal  newval=__sync_or_and_fetch(&p->barrier, 0);
    int ret=(newval&(~FUTEX_WAITERS))==LOCKED_VAL ? 0 : -1;
    LOCKLOGD("AFTER WAIT: %d restoring %d\n", newval, val);
  if(__sync_bool_compare_and_swap(&p->barrier, newval, val)) {
    return ret;
  }
  return -2;
#endif
}

int sb_wake_lock(struct simple_lock_barrier_t *p) {
#ifdef USE_OLD_LOCK
    return sb_wake((simple_barrier_t *)&p->barrier);
#else
  int mythreadid=syscall(SYS_gettid);
  FutexVal val=__sync_val_compare_and_swap(&p->barrier, LOCKED_VAL, UNLOCKED_VAL);
  if (val==LOCKED_VAL) {
      LOCKLOGD("NO WAITERS cued\n");
    return 0;
  }
  if(val&FUTEX_WAITERS) {
      LOCKLOGD("WAITERS cued\n");
    syscall(__NR_futex, &p->barrier, FUTEX_UNLOCK_PI, 1, NULL, NULL, 0, 0);
    return 0;
  }
  else {
      LOCKLOGD("WAKE failed, val = %d, mythread: %d\n", val, mythreadid);
    return -2;
  }
#endif
}

void sb_clobber_lock(struct simple_lock_barrier_t *p) {
#ifdef USE_OLD_LOCK
    return sb_clobber((simple_barrier_t *)&p->barrier);
#else
  int mythreadid=syscall(SYS_gettid);
  int val = LOCKED_VAL;
  //while (val = __sync_val_compare_and_swap(&p->barrier, UNLOCKED_VAL, LOCKED_VAL));
    // Assume multithreaded processing hasn't yet started, brutally assign the lock
    p->barrier=LOCKED_VAL;

  p->runnerThreadId=mythreadid; // Assign it here so that sb_wait_and_reset_lock can reset it when it founds the lock open
#endif
}