#include <err.h>
#include <dirent.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#define verbose(x)

char keycode_to_ascii(char code) {
	if (2 <= code && code <= 10) { /* 1-9 */
		return '1' + code - 2;
	}

	switch (code) {
	case 11: return '0';
	case 30: return 'A';
	case 48: return 'B';
	case 46: return 'C';
	case 32: return 'D';
	case 18: return 'E';
	case 33: return 'F';
	case 28: case 98: return '\012';
	}

	return 0;
}

int try_open_ibutton(char *name) {
	int fd = open(name, O_RDONLY);
	if (fd < 0) {
		warn("open(%s)", name);
		return -1;
	}

	/* query event types, to confirm if it's a keyboard at all */
	int bitmask;
	if (ioctl(fd, EVIOCGBIT(0, sizeof(int)), &bitmask) < 0) {
		warn("ioctl(%s, EVIOCGBIT)", name);
		close(fd);
		return -1;
	}

	if (!(bitmask & (1 << EV_SYN))) {
		verbose(warnx("device %s does not support EV_SYN", name));
		return -1;
	}

	if (!(bitmask & (1 << EV_KEY))) {
		verbose(warnx("device %s does not support EV_KEY, probably is not a keyboard", name));
		return -1;
	}

	if (bitmask & ((1 << EV_REL) | (1 << EV_ABS))) {
		verbose(warnx("device %s supports EV_REL or EV_ABS, a keyboard shouldn't do this", name));
		return -1;
	}

	/* validate device name against known ibutton reader one */

	char device_name[128];
	if (ioctl(fd, EVIOCGNAME(sizeof(device_name) - 1), device_name) < 0) {
		warn("ioctl(%s, EVIOCGNAME)", name);
		close(fd);
		return -1;
	}

	if (strcmp("Generic USB K/B", device_name)) {
		verbose(warnx("device did not identify as ibutton reader (got: \"%s\")", device_name));
		return -1;
	}

	/* all right, grab it */

	if (ioctl(fd, EVIOCGRAB, 1) < 0) {
		warn("ioctl(%s, EVIOCGRAB)", name);
		close(fd);
		return -1;
	}

	warnx("using %s", name);

	return fd;
}

int try_find_ibutton() {
	const char *devdir = "/dev/input";
	int fd = -1;

	DIR *dev_input = opendir(devdir);
	struct dirent *dev;
	while ((dev = readdir(dev_input)) != NULL) {
		if (dev->d_type != DT_CHR) {
			continue;
		}

		size_t len = strlen(devdir) + strlen(dev->d_name) + 2;
		char *fn = alloca(len);
		if (fn == NULL) {
			continue;
		}

		snprintf(fn, len, "%s/%s", devdir, dev->d_name);

		fd = try_open_ibutton(fn);

		if (fd > 0) {
			break;
		}
	}

	closedir(dev_input);

	return fd;
}

int main(int argc, char *argv[]) {
	int fd = try_find_ibutton();

	if (fd < 0) {
		errx(1, "no ibutton reader device found");
	}

	struct input_event ev;
	while (1) {
		int rb = read(fd, &ev, sizeof(struct input_event));

		if (rb < 0) {
			err(1, "read failed");
		}
		if (rb < sizeof(struct input_event)) {
			err(1, "short read");
		}

		if (ev.type == EV_KEY) {
			if (ev.value == 1) { /* key press */
				char c = keycode_to_ascii(ev.code);
				if (c) {
					write(1, &c, 1);
				} else {
					warnx("unknown key type %d code %d value %d", ev.type, ev.code, ev.value);
				}
			}
		}
	}
}

