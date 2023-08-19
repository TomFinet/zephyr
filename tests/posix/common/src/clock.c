/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <zephyr/ztest.h>

#define SLEEP_SECONDS 1
#define CLOCK_INVALID -1

ZTEST(posix_apis, test_clock)
{
	int64_t nsecs_elapsed, secs_elapsed;
	struct timespec ts, te;

	printk("POSIX clock APIs\n");

	/* TESTPOINT: Pass invalid clock type */
	zassert_equal(clock_gettime(CLOCK_INVALID, &ts), -1,
			NULL);
	zassert_equal(errno, EINVAL);

	zassert_ok(clock_gettime(CLOCK_MONOTONIC, &ts));
	zassert_ok(k_sleep(K_SECONDS(SLEEP_SECONDS)));
	zassert_ok(clock_gettime(CLOCK_MONOTONIC, &te));

	if (te.tv_nsec >= ts.tv_nsec) {
		secs_elapsed = te.tv_sec - ts.tv_sec;
		nsecs_elapsed = te.tv_nsec - ts.tv_nsec;
	} else {
		nsecs_elapsed = NSEC_PER_SEC + te.tv_nsec - ts.tv_nsec;
		secs_elapsed = (te.tv_sec - ts.tv_sec - 1);
	}

	/*TESTPOINT: Check if POSIX clock API test passes*/
	zassert_equal(secs_elapsed, SLEEP_SECONDS,
			"POSIX clock API test failed");

	printk("POSIX clock APIs test done\n");
}

ZTEST(posix_apis, test_realtime)
{
	int ret;
	struct timespec rts, mts;
	struct timeval tv;

	ret = clock_gettime(CLOCK_MONOTONIC, &mts);
	zassert_equal(ret, 0, "Fail to get monotonic clock");

	ret = clock_gettime(CLOCK_REALTIME, &rts);
	zassert_equal(ret, 0, "Fail to get realtime clock");

	/* Set a particular time.  In this case, the output of:
	 * `date +%s -d 2018-01-01T15:45:01Z`
	 */
	struct timespec nts;
	nts.tv_sec = 1514821501;
	nts.tv_nsec = NSEC_PER_SEC / 2U;

	/* TESTPOINT: Pass invalid clock type */
	zassert_equal(clock_settime(CLOCK_INVALID, &nts), -1,
			NULL);
	zassert_equal(errno, EINVAL);

	ret = clock_settime(CLOCK_MONOTONIC, &nts);
	zassert_not_equal(ret, 0, "Should not be able to set monotonic time");

	ret = clock_settime(CLOCK_REALTIME, &nts);
	zassert_equal(ret, 0, "Fail to set realtime clock");

	/*
	 * Loop 20 times, sleeping a little bit for each, making sure
	 * that the arithmetic roughly makes sense.  This tries to
	 * catch all of the boundary conditions of the clock to make
	 * sure there are no errors in the arithmetic.
	 */
	int64_t last_delta = 0;
	for (int i = 1; i <= 20; i++) {
		usleep(USEC_PER_MSEC * 90U);
		ret = clock_gettime(CLOCK_REALTIME, &rts);
		zassert_equal(ret, 0, "Fail to read realtime clock");

		int64_t delta =
			((int64_t)rts.tv_sec * NSEC_PER_SEC -
			 (int64_t)nts.tv_sec * NSEC_PER_SEC) +
			((int64_t)rts.tv_nsec - (int64_t)nts.tv_nsec);

		/* Make the delta milliseconds. */
		delta /= (NSEC_PER_SEC / 1000U);

		zassert_true(delta > last_delta, "Clock moved backward");
		int64_t error = delta - last_delta;

		/* printk("Delta %d: %lld\n", i, delta); */

		/* Allow for a little drift upward, but not
		 * downward
		 */
		zassert_true(error >= 90, "Clock inaccurate %d", error);
		zassert_true(error <= 110, "Clock inaccurate %d", error);

		last_delta = delta;
	}

	/* Validate gettimeofday API */
	ret = gettimeofday(&tv, NULL);
	zassert_equal(ret, 0);

	ret = clock_gettime(CLOCK_REALTIME, &rts);
	zassert_equal(ret, 0);

	/* TESTPOINT: Check if time obtained from
	 * gettimeofday is same or more than obtained
	 * from clock_gettime
	 */
	zassert_true(rts.tv_sec >= tv.tv_sec, "gettimeofday didn't"
			" provide correct result");
	zassert_true(rts.tv_nsec >= tv.tv_usec * NSEC_PER_USEC,
			"gettimeofday didn't provide correct result");
}

ZTEST(posix_apis, test_clock_nanosleep_errors_errno) {
	struct timespec rem = {};
	struct timespec req = {};

	/*
	 * invalid parameters
	 */
	zassert_equal(clock_nanosleep(
		CLOCK_MONOTONIC, TIMER_ABSTIME, NULL, NULL), -1);
	zassert_equal(errno, EFAULT);

	/* invalid clock */
	zassert_equal(clock_nanosleep(
		-1, TIMER_ABSTIME, &req, &rem), -1);
	zassert_equal(errno, EINVAL);

	/* NULL request */
	errno = 0;
	zassert_equal(clock_nanosleep(
		CLOCK_MONOTONIC, TIMER_ABSTIME, NULL, &rem), -1);
	zassert_equal(errno, EFAULT);
	/* Expect rem to be the same when function returns */
	zassert_equal(rem.tv_sec, 0, "actual: %d expected: %d", rem.tv_sec, 0);
	zassert_equal(rem.tv_nsec, 0, "actual: %d expected: %d", rem.tv_nsec, 0);

	/* negative times */
	errno = 0;
	req = (struct timespec){.tv_sec = -1, .tv_nsec = 0};
	zassert_equal(clock_nanosleep(
		CLOCK_MONOTONIC, TIMER_ABSTIME, &req, NULL), -1);
	zassert_equal(errno, EINVAL);

	errno = 0;
	req = (struct timespec){.tv_sec = 0, .tv_nsec = -1};
	zassert_equal(clock_nanosleep(
		CLOCK_MONOTONIC, TIMER_ABSTIME, &req, NULL), -1);
	zassert_equal(errno, EINVAL);

	errno = 0;
	req = (struct timespec){.tv_sec = -1, .tv_nsec = -1};
	zassert_equal(clock_nanosleep(
		CLOCK_MONOTONIC, TIMER_ABSTIME, &req, NULL), -1);
	zassert_equal(errno, EINVAL);

	/* nanoseconds too high */
	errno = 0;
	req = (struct timespec){.tv_sec = 0, .tv_nsec = 1000000000};
	zassert_equal(clock_nanosleep(
		CLOCK_MONOTONIC, TIMER_ABSTIME, &req, NULL), -1);
	zassert_equal(errno, EINVAL);

	/*
	 * Valid parameters
	 */
	errno = 0;

	/* Happy path, plus make sure the const input is unmodified */
	req = (struct timespec){.tv_sec = 1, .tv_nsec = 1};
	zassert_equal(clock_nanosleep(
		CLOCK_MONOTONIC, TIMER_ABSTIME, &req, NULL), 0);
	zassert_equal(errno, 0);
	zassert_equal(req.tv_sec, 1);
	zassert_equal(req.tv_nsec, 1);

	/* Sleep for 0.0 s. Expect req & rem to be the same when function returns */
	zassert_equal(clock_nanosleep(
		CLOCK_MONOTONIC, TIMER_ABSTIME, &req, &rem), 0);
	zassert_equal(errno, 0);
	zassert_equal(rem.tv_sec, 0, "actual: %d expected: %d", rem.tv_sec, 0);
	zassert_equal(rem.tv_nsec, 0, "actual: %d expected: %d", rem.tv_nsec, 0);

	/*
	 * req and rem point to the same timespec
	 *
	 * Normative spec says they may be the same.
	 * Expect rem to be zero after returning.
	 */
	req = (struct timespec){.tv_sec = 0, .tv_nsec = 1};
	zassert_equal(clock_nanosleep(
		CLOCK_MONOTONIC, TIMER_ABSTIME, &req, &req), 0);
	zassert_equal(errno, 0);
	zassert_equal(req.tv_sec, 0, "actual: %d expected: %d", req.tv_sec, 0);
	zassert_equal(req.tv_nsec, 0, "actual: %d expected: %d", req.tv_nsec, 0);

	/* Absolute timeout in the past. */
	clock_gettime(CLOCK_MONOTONIC, &req);
	zassert_equal(clock_nanosleep(
		CLOCK_MONOTONIC, TIMER_ABSTIME, &req, NULL), 0);
	zassert_equal(rem.tv_sec, 0, "actual: %d expected: %d", rem.tv_sec, 0);
	zassert_equal(rem.tv_nsec, 0, "actual: %d expected: %d", rem.tv_nsec, 0);

	/* Absolute timeout in the past
	 * relative to the realtime clock. */
	clock_gettime(CLOCK_REALTIME, &req);
	zassert_equal(clock_nanosleep(
		CLOCK_REALTIME, TIMER_ABSTIME, &req, NULL), 0);
	zassert_equal(rem.tv_sec, 0, "actual: %d expected: %d", rem.tv_sec, 0);
	zassert_equal(rem.tv_nsec, 0, "actual: %d expected: %d", rem.tv_nsec, 0);
}

static void common(clockid_t clock_id, const uint32_t s, uint32_t ns)
{
	int r;
	uint64_t now;
	struct timespec rem = {0, 0};
	struct timespec req = {s, ns};

	errno = 0;
	r = clock_nanosleep(clock_id, TIMER_ABSTIME, &req, &rem);
	now = k_cycle_get_64();

	zassert_equal(r, 0, "actual: %d expected: %d", r, 0);
	zassert_equal(errno, 0, "actual: %d expected: %d", errno, 0);
	zassert_equal(req.tv_sec, s, "actual: %d expected: %d", req.tv_sec, s);
	zassert_equal(req.tv_nsec, ns, "actual: %d expected: %d", req.tv_nsec, ns);
	zassert_equal(rem.tv_sec, 0, "actual: %d expected: %d", rem.tv_sec, 0);
	zassert_equal(rem.tv_nsec, 0, "actual: %d expected: %d", rem.tv_nsec, 0);

	uint64_t actual_ns = k_cyc_to_ns_ceil64(now);
	uint64_t exp_ns = (uint64_t)s * NSEC_PER_SEC + ns;
	/* round up to the nearest microsecond for k_busy_wait() */
	exp_ns = DIV_ROUND_UP(exp_ns, NSEC_PER_USEC) * NSEC_PER_USEC;

	/* lower bounds check */
	zassert_true(actual_ns >= exp_ns,
		"actual: %llu expected: %llu", actual_ns, exp_ns);

	/* TODO: Upper bounds check when hr timers are available */
}

ZTEST(posix_apis, test_clock_nanosleep_execution)
{
	/* sleep until absolute 1s + 1ns */
	common(CLOCK_MONOTONIC, 1, 1);

	/* sleep until absolute 1s + 1us */
	common(CLOCK_MONOTONIC, 1, 1000);

	/* sleep for 500000000ns */
	common(CLOCK_MONOTONIC, 1, 500000000);

	/* sleep until absolute 2s */
	common(CLOCK_MONOTONIC, 2, 0);

	/* sleep until absolute 2s + 1ns */
	common(CLOCK_MONOTONIC, 2, 1);

	/* sleep until absolute 2s + 1us + 1ns */
	common(CLOCK_MONOTONIC, 2, 1001);

	/* set realtime clock base to 1s */
	const struct timespec ts = {1, 0};
	clock_settime(CLOCK_REALTIME, &ts);

	/* absolute sleep until realtime 4s + 1ns */
	common(CLOCK_REALTIME, ts.tv_sec + 3, 1);

	/* absolute sleep until realtime 4s + 1us */
	common(CLOCK_REALTIME, ts.tv_sec + 3, 1000);

	/* absolute sleep until realtime 4s + 500000000ns */
	common(CLOCK_REALTIME, ts.tv_sec + 3, 500000000);

	/* absolute sleep until realtime 5s */
	common(CLOCK_REALTIME, ts.tv_sec + 4, 0);

	/* absolute sleep until realtime 5s + 1ns */
	common(CLOCK_REALTIME, ts.tv_sec + 4, 1);

	/* absolute sleep until realtime 5s + 1us + 1ns */
	common(CLOCK_REALTIME, ts.tv_sec + 4, 1001);
}