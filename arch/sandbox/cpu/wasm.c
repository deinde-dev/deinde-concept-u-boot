// SPDX-License-Identifier: GPL-2.0+
/*
 * Support for running sandbox as WebAssembly
 *
 * This file is built with the system headers.
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <emscripten.h>
#include <errno.h>
#include <unistd.h>
#include <os.h>

/*
 * Generic code uses this as the start of the U-Boot image. Code is not in
 * the address space with WebAssembly, so this is only a placeholder.
 */
void _start(void)
{
}

void os_wasm_sleep(unsigned long usec)
{
	/*
	 * This returns to the JavaScript event loop, so that a web page can
	 * handle keypresses and update its display. A browser cannot block.
	 */
	emscripten_sleep((usec + 999) / 1000);
}

/* Emscripten's C library lacks this, which some tests use */
ssize_t getrandom(void *buf, size_t size, unsigned int flags)
{
	size_t done, todo;

	/* getentropy() provides at most 256 bytes at a time */
	for (done = 0; done < size; done += todo) {
		todo = size - done;
		if (todo > 256)
			todo = 256;
		if (getentropy(buf + done, todo))
			return -1;
	}

	return size;
}
