// this is the aquarium daemon
// we don't want only the root user to be able to create/run programs inside aquariums, so this daemon, which runs as root, basically waits until another process asks it to create a new aquarium from a sanctioned list of images
// this process can be a user process, so long as that user is part of the "stoners" group (the group allowed to create aquariums)
// it also creates a user in the aquarium with the same privileges and home directory (*only if asked, it can also create a separate home directory) as the user which asked for the aquarium to be created

// awesome video on message queues: https://www.youtube.com/watch?v=OYqX19lPb0A
// (totally underrated channel/guy btw)

#include <bob.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <err.h>
#include <mqueue.h>

#define MQ_NAME "/aquariumd"

#define MAX_MESSAGES 10 // not yet sure what this signifies
#define MESSAGE_SIZE 256

#define OP_CREATE_AQUARIUM 0x63
#define OP_DELETE_AQUARIUM 0x64

// in the future, it may be interesting to add some more of these (e.g. so that aquariums don't have access to '/dev' - you'd want to restrict this access because, even if you trust the aquarium, you're considerably increasing the attack surface in case for e.g. a root service is exploited by a misbehaving user process)

#define FLAG_LINK_HOME 0b01
#define FLAG_LINK_TMP  0b10

typedef struct {
	uint8_t op;
	uint8_t flags;
	uint8_t name[256];
} cmd_t;

static inline void __process_cmd(cmd_t* cmd) {
	if (cmd->flags & FLAG_LINK_HOME) {
		errx(EXIT_FAILURE, "Command's 'FLAG_LINK_HOME' flag set\n"); // TODO way to *securely* get the user which sent this command?
	}

	// TODO
}

int main(void) {
	// TODO make sure another aquariumd process isn't already running

	// TODO make sure the "stoners" group exists, and error if not

	// make sure a message queue named $MQ_NAME doesn't already exist

	mode_t permissions = 0420; // owner ("root") can only read, group ("stoners") can only write, and others can do neither - I swear it's a complete coincidence this ends up as 420 in octal - at least I ain't finna forget the permission numbers any time soon 🤣

	struct mq_attr attr = {
		.mq_flags = O_BLOCK,
		.mq_maxmsg = MAX_MESSAGES,
		.mq_msgsize = MESSAGE_SIZE,
		.mq_curmsgs = 0,
	};

	if (mq_open(MQ_NAME, O_CREAT | O_EXCL, permissions, &attr) < 0) {
		if (errno == EEXIST) {
			errx(EXIT_FAILURE, "Message queue named \"" MQ_NAME "\" already exists");
		}

		errx(EXIT_FAILURE, "Failed detecting message queue: %s", strerror(errno));
	}

	// create message queue

	mqd_t mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, permissions, attr);

	if (mq < 0) {
		errx(EXIT_FAILURE, "Failed to create message queue: %s", strerror(errno));
	}

	// block while waiting for messages on the message queue

	while (1) {
		cmd_t cmd;
		__attribute__((unused)) int priority; // we don't care about priority

		ssize_t len = mq_receive(mq, &cmd, sizeof cmd, &priority);

		if (errno == EAGAIN) {
			continue;
		}

		if (errno == ETIMEDOUT) {
			errx(EXIT_FAILURE, "Receiving on the message queue timed out");
		}

		if (len < 0) {
			errx(EXIT_FAILURE, "mq_receive: %s", strerror(errno));
		}

		// received a message

		__process_cmd(&cmd);
	}

	// don't forget to remove the message queue completely

	mq_close(mq); // not sure if this is completely necessary, doesn't matter
	mq_unlink(MQ_NAME);

	return 0;
}