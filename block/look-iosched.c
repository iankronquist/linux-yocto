#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list_sort.h>

struct look_data {
	struct list_head queue;
	//struct list_head *cur_pos;
};


// Choose the next request to service
static int look_dispatch(struct request_queue *q, int force)
{
	// Get the current position of the read head
	sector_t position = rq_end_sector(rq);

	// For each entry
	struct list_head *entry;
	list_for_each(entry, &ld->queue) {
		// Unwrap the current request and get its position
		struct request *rq = list_entry(entry, struct request, queuelist);
		position_i = blk_rq_pos(rqi);
		// If the position in the queue is greater than the current one
		if (position_i > position) {
			//ld->cur_pos = entry;
			// Go backward one step
			entry = list_prev_entry(entry);
			rq = list_entry(entry, struct request, queuelist);

			// Remove the entry
			list_del(entry);
			// Dispatch the request
			elv_dispatch_sort(q, rq);
			return 1;
		}
	}
	// If we couldn't find any requests starting at the beginning
	return 0;
}

// From plug_rq_cmp in  block/blk-core.c
static int rq_cmp(void *priv, struct list_head *a, struct list_head *b)
{
	struct request *rqa = container_of(a, struct request, queuelist);
	struct request *rqb = container_of(b, struct request, queuelist);

	return blk_rq_pos(rqa) - blk_rq_pos(rqb);
}

// Use the elv_attempt_insert_merge function to insert the request rq into the queue
// q in the right spot.
static void look_add_request(struct request_queue *q, struct request *rq)
{
	// The elevator data is a void*
	struct noop_data *nd = q->elevator->elevator_data;

	/*
	// Acquire the lock
	spin_lock_irq(q->queue_lock);
	// Attempt to merge the request into an existing request. Returns true if
	// it succeeded.
	bool merged = elv_attempt_insert_merge(r, rq);
	spin_unlock_irq(q->queue_lock);
	 */
	// If we didn't merge, insert it into the list in sorted order.
	//if (!merged) {
	list_add(&rq->queuelist, &nd->queue);
	list_sort(NULL, &nq->queue, rq_cmp);
	//}
}


static int look_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct look_data *ld;
	struct elevator_queue *eq;
	// Allocate the elevator
	eq = elevator_alloc(q, e);
	if (eq == NULL)
		return -ENOMEM;

	// Allocate memory for the look data.
	ld = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->nore);
	if (ld == NULL) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	// Set the elevator queue's data to the look data
	eq->elevator_data = ld;

	// Initialize the look data queue
	INIT_LIST_HEAD(&ld->queue);
	ld->cur_pos = &ld->queue;
	// Acquire the lock and set the queue's elevator queue
	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);
	return 0;
}

static void look_exit_queue(struct elevator_queue *e)
{
	struct look_data *ld = e->elevator_data;
	// If we're trying to exit and we have an empty queue there's a bug.
	BUG_ON(!list_empty(&ld->queue));
	// Free the look data
	kfree(ld);
}

static struct elevator_type elevator_look = {
	.ops = {
		// Called to see if bio can be merged with an already pending request
		//        .elevator_merge_fn          = look_merge,
		// Called after requests are merged
		//		.elevator_merge_req_fn		= look_merged_requests,
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
