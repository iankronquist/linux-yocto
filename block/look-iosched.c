#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list_sort.h>

#define DEBUG 1

struct look_data {
	struct list_head queue;
	sector_t last_serviced;
};

void print_list(struct look_data *ld) {
	struct list_head *entry = &ld->queue;
	printk("[");
	if (!list_empty(&ld->queue)) {
		list_for_each(entry, &ld->queue) {
			struct request *rq = list_entry(entry, struct request, queuelist);
			printk("%llx, ", blk_rq_pos(rq));
		}
	}
	printk("]\n");
}


// Choose the next request to service
static int look_dispatch(struct request_queue *q, int force)
{
	sector_t position;
	struct list_head *entry;
	// The elevator data is a void*
	struct look_data *ld = q->elevator->elevator_data;
#if DEBUG
	printk("look_dispatch\n");
#endif
#if DEBUG
	print_list(ld);
#endif
	if (!list_empty(&ld->queue)) {
		struct request *rq;
		rq = list_entry(ld->queue.next, struct request, queuelist);
		// Get the current position of the read head
		// For each entry
		list_for_each(entry, &ld->queue) {
			// Unwrap the current request and get its position
			struct request *rq = list_entry(entry, struct request, queuelist);
			position = blk_rq_pos(rq);
			// If the position in the queue is greater than the current one
			if (position > ld->last_serviced) {
				// Go backward one step
				entry = entry->prev;
				rq = list_entry(entry, struct request, queuelist);
				position = blk_rq_pos(rq);
				break;
			}
		}
		// Remove the entry
		list_del_init(&rq->queuelist);
		// Dispatch the request
		elv_dispatch_sort(q, rq);
		// Update the last serviced position
		ld->last_serviced = position;
		return 1;
	}
	return 0;
}

// From plug_rq_cmp in  block/blk-core.c
static int rq_cmp(void *priv, struct list_head *a, struct list_head *b)
{
	struct request *rqa = container_of(a, struct request, queuelist);
	struct request *rqb = container_of(b, struct request, queuelist);

#if DEBUG
	printk("rq_cmp\n");
#endif
	return blk_rq_pos(rqa) - blk_rq_pos(rqb);
}

// Use the elv_attempt_insert_merge function to insert the request rq into the queue
// q in the right spot.
static void look_add_request(struct request_queue *q, struct request *rq)
{
	// The elevator data is a void*
	struct look_data *ld = q->elevator->elevator_data;

#if DEBUG
	printk("look_add_request\n");
#endif

	list_add(&rq->queuelist, &ld->queue);
	list_sort(NULL, &ld->queue, rq_cmp);
}


static int look_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct look_data *ld;
	struct elevator_queue *eq;
#if DEBUG
	printk("look_init_queue\n");
#endif
	// Allocate the elevator
	eq = elevator_alloc(q, e);
	if (eq == NULL)
		return -ENOMEM;

	// Allocate memory for the look data.
	ld = kmalloc_node(sizeof(*ld), GFP_KERNEL, q->node);
	if (ld == NULL) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	// Set the elevator queue's data to the look data
	eq->elevator_data = ld;

	// Initialize the look data queue
	INIT_LIST_HEAD(&ld->queue);
	//ld->cur_pos = &ld->queue;
	// Acquire the lock and set the queue's elevator queue
	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);
	ld->last_serviced = 0;
	return 0;
}

static int look_merge(struct request_queue *q, struct request **req,
		struct bio *bio)
{
	sector_t position;
	struct list_head *entry;
	struct look_data *ld = q->elevator->elevator_data;
	sector_t sect = bio_end_sector(bio);
#if DEBUG
	printk("look_merge\n");
#endif
	if (!list_empty(&ld->queue)) {
		// For each entry
		list_for_each(entry, &ld->queue) {
			struct request *rq = list_entry(entry, struct request, queuelist);
			position = blk_rq_pos(rq);
			printk("thinking of merging %llx and %llx\n", position, sect);
			if (position == sect) {
				*req = rq;
#if DEBUG
				printk("Front merging %llx and %llx\n", position, sect);
#endif
				return ELEVATOR_FRONT_MERGE;
			}
		}
	}
#if DEBUG
	printk("No merge necessary\n");
#endif
	return ELEVATOR_NO_MERGE;
}

static void look_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
#if DEBUG
	printk("look_merged_requests\n");
#endif
	list_del_init(&next->queuelist);
}

static void look_exit_queue(struct elevator_queue *e)
{
	struct look_data *ld = e->elevator_data;
#if DEBUG
	printk("look_exit_queue\n");
#endif
	// If we're trying to exit and we have an empty queue there's a bug.
	BUG_ON(!list_empty(&ld->queue));
	// Free the look data
	kfree(ld);
}

static struct elevator_type elevator_look = {
	.ops = {
		// Called to see if bio can be merged with an already pending request
		.elevator_merge_fn          = look_merge,
		// Called after requests are merged
		.elevator_merge_req_fn		= look_merged_requests,
		// Called when readying the next request for the driver
		.elevator_dispatch_fn		= look_dispatch,
		// Add a new request to the queue
		.elevator_add_req_fn		= look_add_request,
		/*
		// Get the next request
		.elevator_former_req_fn		= elv_former_request,
		// Get the previous request
		.elevator_latter_req_fn		= elv_latter_request,
		*/
		.elevator_init_fn		= look_init_queue,
		.elevator_exit_fn		= look_exit_queue,
	},
	.elevator_name = "look",
	.elevator_owner = THIS_MODULE,
};

static int __init look_init(void)
{
	return elv_register(&elevator_look);
}

static void __exit look_exit(void)
{
	elv_unregister(&elevator_look);
}

module_init(look_init);
module_exit(look_exit);


MODULE_AUTHOR("Ian Kronquist");
MODULE_LICENSE("Dual BSD GPL");
MODULE_DESCRIPTION("Look I/O scheduler");
