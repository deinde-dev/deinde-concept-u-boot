// SPDX-License-Identifier: GPL-2.0+
/*
 * Emulated NVMe controller for sandbox
 *
 * This presents the register and queue interface which drivers/nvme/nvme.c
 * expects, so that the real driver runs unchanged. Data is held in a file on
 * the host, named by the 'filename' property.
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_NVME

#include <blk.h>
#include <dm.h>
#include <log.h>
#include <malloc.h>
#include <mapmem.h>
#include <os.h>
#include <asm/state.h>
#include <asm/test.h>
#include <linux/sizes.h>
#include "nvme.h"

/* the doorbells start one page into the BAR, as the spec requires */
#define SBN_DB_OFFSET		4096
#define SBN_BAR_SIZE		(SBN_DB_OFFSET + SZ_4K)

/* what the emulated controller reports */
#define SBN_MAX_QUEUE		64
#define SBN_BLOCK_SHIFT		9
#define SBN_BLOCK_SIZE		BIT(SBN_BLOCK_SHIFT)
#define SBN_NUM_QUEUES		2

/* size used when no backing file is named */
#define SBN_DEFAULT_SIZE	SZ_2M

/* controller-ready timeout the driver is told to allow, in 500ms units */
#define SBN_CAP_TIMEOUT		2

/**
 * struct sbn_queue - One submission/completion queue pair
 *
 * @sq: Submission queue in the driver's memory, or NULL if not created
 * @cq: Completion queue in the driver's memory, or NULL if not created
 * @depth: Number of entries in each
 * @sq_head: Entry the emulator will read next
 * @cq_tail: Entry the emulator will write next
 * @phase: Phase tag to write, flipped each time the queue wraps
 */
struct sbn_queue {
	struct nvme_command *sq;
	struct nvme_completion *cq;
	uint depth;
	uint sq_head;
	uint cq_tail;
	uint phase;
};

/**
 * struct sbn_priv - Private data for the emulated controller
 *
 * @ndev: State for the real driver, which must come first since nvme.c
 *	reaches it with dev_get_priv(), as apple_nvme_priv does
 * @bar: The emulated BAR, which the driver reads and writes
 * @cc: Controller Configuration, as last written
 * @csts: Controller Status, which the driver polls
 * @aqa: Admin Queue Attributes
 * @asq: Admin submission-queue address
 * @acq: Admin completion-queue address
 * @queue: The queues, 0 being the admin pair
 * @buf: The data, either a mapped file or memory of our own
 * @mapped: true if @buf came from a file and must be unmapped
 * @size: Size of @buf in bytes
 * @blocks: Size of @buf in blocks
 */
struct sbn_priv {
	struct nvme_dev ndev;
	void *bar;
	u32 cc;
	u32 csts;
	u32 aqa;
	u64 asq;
	u64 acq;
	struct sbn_queue queue[SBN_NUM_QUEUES];
	u8 *buf;
	bool mapped;
	int size;
	lbaint_t blocks;
};

/**
 * sbn_complete() - Post a completion for a command
 *
 * @q: Queue the command came from
 * @qid: Its ID, which goes in the completion
 * @cmd: The command being completed
 * @status: NVMe status code to report
 * @result: Value for the result field, used by Set Features
 */
static void sbn_complete(struct sbn_queue *q, uint qid,
			 const struct nvme_command *cmd, u16 status,
			 u32 result)
{
	struct nvme_completion *cqe;

	if (!q->cq)
		return;
	cqe = &q->cq[q->cq_tail];
	cqe->result = cpu_to_le32(result);
	cqe->sq_head = cpu_to_le16(q->sq_head);
	cqe->sq_id = cpu_to_le16(qid);
	cqe->command_id = cmd->common.command_id;

	/*
	 * The driver spots a new entry by the phase bit differing from the
	 * one it saw last time round the queue
	 */
	cqe->status = cpu_to_le16((status << 1) | q->phase);

	if (++q->cq_tail == q->depth) {
		q->cq_tail = 0;
		q->phase ^= 1;
	}
}

/**
 * sbn_identify() - Handle an Identify command
 *
 * Fills in only the fields the driver and 'nvme detail' actually read
 *
 * @priv: Private data
 * @cmd: The command
 * Return: NVMe status code
 */
static u16 sbn_identify(struct sbn_priv *priv, const struct nvme_command *cmd)
{
	void *buf = nomap_sysmem(le64_to_cpu(cmd->identify.prp1), SZ_4K);
	uint cns = le32_to_cpu(cmd->identify.cns);

	memset(buf, '\0', SZ_4K);
	if (cns == 1) {
		struct nvme_id_ctrl *ctrl = buf;

		memcpy(ctrl->mn, "sandbox-nvme", 12);
		memcpy(ctrl->sn, "0123456789ab", 12);
		memcpy(ctrl->fr, "1.0", 3);
		ctrl->nn = cpu_to_le32(1);
		ctrl->vwc = 0;

		/* no limit on transfer size, so the driver uses its own */
		ctrl->mdts = 0;
	} else if (cns == 0) {
		struct nvme_id_ns *id = buf;

		id->nsze = cpu_to_le64(priv->blocks);
		id->ncap = id->nsze;
		id->nuse = id->nsze;
		id->nlbaf = 0;	/* one format, described below */
		id->flbas = 0;
		id->lbaf[0].ds = SBN_BLOCK_SHIFT;
	} else {
		return NVME_SC_INVALID_FIELD;
	}

	return NVME_SC_SUCCESS;
}

/**
 * sbn_rw() - Handle a Read or Write command against the backing file
 *
 * @priv: Private data
 * @cmd: The command
 * @read: true to read from the file, false to write to it
 * Return: NVMe status code
 */
static u16 sbn_rw(struct sbn_priv *priv, const struct nvme_command *cmd,
		  bool read)
{
	u64 slba = le64_to_cpu(cmd->rw.slba);
	uint blocks = le16_to_cpu(cmd->rw.length) + 1;
	ulong bytes = blocks * SBN_BLOCK_SIZE;
	void *buf;

	if (slba + blocks > priv->blocks)
		return NVME_SC_LBA_RANGE;

	/*
	 * The driver builds a PRP list for anything longer than two pages,
	 * which this does not read, so refuse what it cannot honour rather
	 * than quietly transferring the wrong thing
	 */
	if (bytes > 2 * SZ_4K)
		return NVME_SC_INVALID_FIELD;

	buf = nomap_sysmem(le64_to_cpu(cmd->rw.prp1), bytes);
	if (read)
		memcpy(buf, priv->buf + slba * SBN_BLOCK_SIZE, bytes);
	else
		memcpy(priv->buf + slba * SBN_BLOCK_SIZE, buf, bytes);

	return NVME_SC_SUCCESS;
}

/**
 * sbn_exec() - Run one command and post its completion
 *
 * @priv: Private data
 * @qid: Queue the command came from
 * @cmd: The command
 */
static void sbn_exec(struct sbn_priv *priv, uint qid,
		     const struct nvme_command *cmd)
{
	struct sbn_queue *q = &priv->queue[qid];
	u16 status = NVME_SC_SUCCESS;
	u32 result = 0;

	if (qid) {
		/* an I/O queue carries only reads and writes */
		switch (cmd->common.opcode) {
		case nvme_cmd_read:
			status = sbn_rw(priv, cmd, true);
			break;
		case nvme_cmd_write:
			status = sbn_rw(priv, cmd, false);
			break;
		default:
			status = NVME_SC_INVALID_OPCODE;
			break;
		}
	} else {
		switch (cmd->common.opcode) {
		case nvme_admin_identify:
			status = sbn_identify(priv, cmd);
			break;
		case nvme_admin_create_cq: {
			uint id = le16_to_cpu(cmd->create_cq.cqid);

			if (id >= SBN_NUM_QUEUES) {
				status = NVME_SC_INVALID_FIELD;
				break;
			}
			priv->queue[id].cq =
				nomap_sysmem(le64_to_cpu(cmd->create_cq.prp1), SZ_4K);
			priv->queue[id].depth =
				le16_to_cpu(cmd->create_cq.qsize) + 1;
			priv->queue[id].cq_tail = 0;
			priv->queue[id].phase = 1;
			break;
		}
		case nvme_admin_create_sq: {
			uint id = le16_to_cpu(cmd->create_sq.sqid);

			if (id >= SBN_NUM_QUEUES) {
				status = NVME_SC_INVALID_FIELD;
				break;
			}
			priv->queue[id].sq =
				nomap_sysmem(le64_to_cpu(cmd->create_sq.prp1), SZ_4K);
			priv->queue[id].sq_head = 0;
			break;
		}
		case nvme_admin_set_features:
			/* report the queue count which was asked for */
			result = le32_to_cpu(cmd->features.dword11);
			break;
		case nvme_admin_get_features:
			break;
		default:
			status = NVME_SC_INVALID_OPCODE;
			break;
		}
	}

	sbn_complete(q, qid, cmd, status, result);
}

/**
 * sbn_doorbell() - Handle a write to a doorbell register
 *
 * A submission doorbell says how far the driver has filled the queue, so run
 * everything up to that point. A completion doorbell only says what the driver
 * has consumed, which this emulator does not need to track.
 *
 * @priv: Private data
 * @offset: Offset of the doorbell within the BAR
 * @val: Value written, which is the new queue index
 */
static void sbn_doorbell(struct sbn_priv *priv, ulong offset, uint val)
{
	uint index = (offset - SBN_DB_OFFSET) / sizeof(u32);
	uint qid = index / 2;
	struct sbn_queue *q;

	if (qid >= SBN_NUM_QUEUES || (index & 1))
		return;
	q = &priv->queue[qid];
	if (!q->sq || !q->depth)
		return;

	while (q->sq_head != val) {
		sbn_exec(priv, qid, &q->sq[q->sq_head]);
		if (++q->sq_head == q->depth)
			q->sq_head = 0;
	}
}

static long sbn_read(void *ctx, const void *addr, enum sandboxio_size_t size)
{
	struct udevice *dev = ctx;
	struct sbn_priv *priv = dev_get_priv(dev);
	ulong offset = (ulong)addr - (ulong)priv->bar;

	switch (offset) {
	case offsetof(struct nvme_bar, cap):
		/*
		 * MQES in the low bits, then a timeout in units of 500ms:
		 * the driver multiplies that out and gives up at once if it
		 * is zero. The stride and minimum page size are both left at
		 * zero, meaning 4-byte doorbells and 4KB pages.
		 */
		return (SBN_MAX_QUEUE - 1) | ((u64)SBN_CAP_TIMEOUT << 24);
	case offsetof(struct nvme_bar, vs):
		return 0x00010400;	/* 1.4.0 */
	case offsetof(struct nvme_bar, cc):
		return priv->cc;
	case offsetof(struct nvme_bar, csts):
		return priv->csts;
	case offsetof(struct nvme_bar, aqa):
		return priv->aqa;
	}

	return 0;
}

static void sbn_write(void *ctx, void *addr, unsigned int val,
		      enum sandboxio_size_t size)
{
	struct udevice *dev = ctx;
	struct sbn_priv *priv = dev_get_priv(dev);
	ulong offset = (ulong)addr - (ulong)priv->bar;

	if (offset >= SBN_DB_OFFSET) {
		sbn_doorbell(priv, offset, val);
		return;
	}

	switch (offset) {
	case offsetof(struct nvme_bar, cc):
		priv->cc = val;

		/* the driver waits for ready to follow enable either way */
		if (val & NVME_CC_ENABLE) {
			struct sbn_queue *q = &priv->queue[0];

			q->depth = (priv->aqa & 0xfff) + 1;
			q->sq = nomap_sysmem(priv->asq, SZ_4K);
			q->cq = nomap_sysmem(priv->acq, SZ_4K);
			q->sq_head = 0;
			q->cq_tail = 0;
			q->phase = 1;
			priv->csts |= NVME_CSTS_RDY;
		} else {
			priv->csts &= ~NVME_CSTS_RDY;
		}
		break;
	case offsetof(struct nvme_bar, aqa):
		priv->aqa = val;
		break;
	case offsetof(struct nvme_bar, asq):
		priv->asq = (priv->asq & ~0xffffffffULL) | val;
		break;
	case offsetof(struct nvme_bar, asq) + 4:
		priv->asq = (priv->asq & 0xffffffffULL) | ((u64)val << 32);
		break;
	case offsetof(struct nvme_bar, acq):
		priv->acq = (priv->acq & ~0xffffffffULL) | val;
		break;
	case offsetof(struct nvme_bar, acq) + 4:
		priv->acq = (priv->acq & 0xffffffffULL) | ((u64)val << 32);
		break;
	}
}

static int sandbox_nvme_remove(struct udevice *dev);

static int sandbox_nvme_probe(struct udevice *dev)
{
	struct sbn_priv *priv = dev_get_priv(dev);
	const char *fname;
	int ret;

	priv->bar = memalign(SZ_4K, SBN_BAR_SIZE);
	if (!priv->bar)
		return log_msg_ret("sbb", -ENOMEM);
	memset(priv->bar, '\0', SBN_BAR_SIZE);

	fname = dev_read_string(dev, "filename");
	if (fname) {
		char buf[256];

		/* keep the data with the other sandbox state where possible */
		if (!os_persistent_file(buf, sizeof(buf), fname))
			fname = buf;
		ret = os_map_file(fname, OS_O_RDWR | OS_O_CREAT,
				  (void **)&priv->buf, &priv->size);
		priv->mapped = true;
		if (ret) {
			log_err("%s: Unable to map file '%s'\n", dev->name,
				fname);
			free(priv->bar);

			return ret;
		}
	} else {
		/* with no file the device is still useful, just not durable */
		priv->size = dev_read_u32_default(dev, "sandbox,size",
						  SBN_DEFAULT_SIZE);
		priv->buf = calloc(1, priv->size);
		if (!priv->buf) {
			free(priv->bar);

			return log_msg_ret("sbd", -ENOMEM);
		}
	}
	priv->blocks = priv->size >> SBN_BLOCK_SHIFT;

	/*
	 * The driver reads completion entries with readw(), which sandbox
	 * sends through sandbox_read(). Those are in ordinary memory rather
	 * than the emulated BAR, so without this they read back as zero and
	 * no completion is ever seen.
	 */
	sandbox_set_enable_memio(true);

	ret = sandbox_mmio_add(priv->bar, SBN_BAR_SIZE, sbn_read, sbn_write,
			       dev);
	if (ret) {
		free(priv->bar);
		return log_msg_ret("sbm", ret);
	}

	/* hand the BAR to the real driver and let it bring the device up */
	priv->ndev.bar = priv->bar;

	ret = nvme_init(dev);
	if (ret) {
		sandbox_nvme_remove(dev);

		return log_msg_ret("sbi", ret);
	}

	return 0;
}

static int sandbox_nvme_remove(struct udevice *dev)
{
	struct sbn_priv *priv = dev_get_priv(dev);

	/*
	 * The region must go when the device does. Leaving it registered
	 * would shadow whatever the allocator hands out next, which shows up
	 * as another driver reading zeroes from its own memory
	 */
	sandbox_mmio_remove(dev);
	free(priv->bar);
	priv->bar = NULL;

	/*
	 * The data must go too. A device-model test rebuilds the tree for
	 * every test, so anything left here is lost a few hundred times over
	 * and the malloc pool runs out
	 */
	if (priv->mapped)
		os_unmap(priv->buf, priv->size);
	else
		free(priv->buf);
	priv->buf = NULL;

	return 0;
}

static const struct udevice_id sandbox_nvme_ids[] = {
	{ .compatible = "sandbox,nvme" },
	{ }
};

U_BOOT_DRIVER(sandbox_nvme) = {
	.name	= "sandbox_nvme",
	.id	= UCLASS_NVME,
	.of_match = sandbox_nvme_ids,
	.probe	= sandbox_nvme_probe,
	.remove	= dm_remove_ptr(sandbox_nvme_remove),
	.priv_auto = sizeof(struct sbn_priv),
};
