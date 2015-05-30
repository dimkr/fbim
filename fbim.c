/*
 * this file is part of fbim.
 *
 * Copyright (c) 2015 Dima Krasner
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <math.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_LINEAR
#include "stb/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_SATURATE_INT
#include "stb/stb_image_resize.h"

struct rgba {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
} __attribute__((packed));

int main(int argc, char *argv[])
{
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	size_t len;
	size_t line_len;
	const char *fb;
	const char *image;
	unsigned char *fb_data;
	unsigned char *im_data;
	unsigned char *resized_data;
	uint32_t *pixel;
	struct rgba *rgba;
	int ret = EXIT_FAILURE;
	int fd;
	int x;
	int y;
	int n;
	int new_x;
	int new_y;
	uint32_t red_len;
	uint32_t blue_len;
	uint32_t green_len;
	uint32_t transp_len;

	switch (argc) {
		case 2:
			fb = "/dev/fb0";
			image = argv[1];
			break;

		case 3:
			fb = argv[1];
			image = argv[2];
			break;

		default:
			(void) fprintf(stderr, "Usage: %s [FB] PATH\n", argv[0]);
			goto end;
	}

	fd = open(fb, O_RDWR);
	if (-1 == fd)
		goto end;

	if (-1 == ioctl(fd, FBIOGET_FSCREENINFO, &fix))
		goto close_fd;

	if (-1 == ioctl(fd, FBIOGET_VSCREENINFO, &var))
		goto close_fd;

	/* we do not support 1/8/16/24 BPP framebuffers at the moment */
	if (32 != var.bits_per_pixel)
		goto close_fd;

	len = fix.line_length * var.yres;
	fb_data = (unsigned char *) mmap(NULL,
	                                 len,
	                                 PROT_READ | PROT_WRITE,
	                                 MAP_SHARED,
	                                 fd,
	                                 0);
	if (MAP_FAILED == fb_data)
		goto end;

	im_data = stbi_load(image, &x, &y, &n, 4);
	if (NULL == im_data)
		goto unmap_fd;

	if ((var.xres >= x) && (var.yres >= y))
		resized_data = im_data;
	else {
		resized_data = malloc(len);
		if (NULL == resized_data)
			goto unmap_fd;

		/* shrink the image and keep the ratio */
		if (x > y) {
			new_x = (int) var.xres;
			new_y = (int) roundf((float) (y * var.xres / x));
		}
		else {
			new_y = (int) var.yres;
			new_x = (int) roundf((float) (x * var.yres / y));
		}

		if (1 != stbir_resize_uint8(im_data,
		                            x,
		                            y,
		                            0,
		                            resized_data,
		                            new_x,
		                            new_y,
		                            0,
		                            n))
			goto free_resized;

		x = new_x;
		y = new_y;
	}

	/* convert the pixels from RGBA to the framebuffer format */
	red_len = 8 - var.red.length;
	green_len = 8 - var.green.length;
	blue_len = 8 - var.blue.length;
	transp_len = 8 - var.transp.length;
	for (pixel = (uint32_t *) resized_data;
	     (uint32_t *) (resized_data + (x * y * n)) > pixel;
	     ++pixel) {
		rgba = (struct rgba *) pixel;
		*pixel = ((rgba->r >> red_len) << var.red.offset) |
		         ((rgba->g >> green_len) << var.green.offset) |
		         ((rgba->b >> blue_len) << var.blue.offset) |
		         ((rgba->a >> transp_len) << var.transp.offset);
	}

	/* calculate the size of single image line, which is different from the size
	 * of a framebuffer line if their resolutions differ */
	line_len = (size_t) (x * n);

	/* y is one-based, so we substract 1 */
	for (--y ; 0 <= y; --y) {
		(void) memcpy((void *) &fb_data[y * fix.line_length],
		              (void *) &resized_data[y * line_len],
		              line_len);
	}

	ret = EXIT_SUCCESS;

	stbi_image_free((void *) im_data);

free_resized:
	if (im_data != resized_data)
		free(resized_data);

unmap_fd:
	(void) munmap(fb_data, len);

close_fd:
	(void) close(fd);

end:
	return ret;
}
