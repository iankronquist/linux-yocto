#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/rbtree.h>

#define DEBUG 1
#define SSTF 1

struct look_data {
	struct rb_root root;
	sector_t last_serviced;
};

/*
void print_list(struct look_data *ld) {
	struct request *rq, *next_rq;
	printk("[");
	rbtree_postorder_for_each_entry_safe(rq, next_rq, ld->root, node) {
		printk("%llx, ", blk_rq_pos(rq));
	}
	printk("]\n");
}
*/

// Walk the rbtree starting at root and find the closest value to sector
// Returns NULL if the tree is empty
// The macro SSTF controls whether we consider branches less than sector.
// Set SSTF to 1 to make the algorithm SSTF
// Setting it to 0 makes the algorithm a C-Look variant
static struct request *
look_elv_rb_find_closest(struct rb_root *root, sector_t sector)
{
	struct rb_node *n = root->rb_node;
	struct request *rq, *closest_rq;
	sector_t closest_pos_diff, pos;

	if (n == NULL) {
		return NULL;
	}
	// Start the search with the root
	closest_rq = rb_entry(n, struct request, rb_node);
	pos = blk_rq_pos(closest_rq);
	closest_pos_diff = sector > pos ? sector - pos : pos - sector;
	// If the difference from the root position is 0, return the root
	if (unlikely(closest_pos_diff == 0)) {
		return closest_rq;
	}

	while (n) {
		rq = rb_entry(n, struct request, rb_node);
		pos = blk_rq_pos(rq);

		if (sector < pos) {
			n = n->rb_left;
			if (pos - sector <  closest_pos_diff) {
				closest_pos_diff = pos - sector;
				closest_rq = rq;
			}
		} else if (sector > pos) {
			n = n->rb_right;
#if SSTF
			if (sector - pos <  closest_pos_diff) {
				closest_pos_diff = sector - pos;
				closest_rq = rq;
			}
#endif
		} else {
			// You can't get any closer than 0.
			return rq;
		}
	}
	return closest_rq;
}

// Choose the next request to service
static int look_dispatch(struct request_queue *q, int force)
{
	struct request *rq;
	// The elevator data is a void*
	struct look_data *ld = q->elevator->elevator_data;
#if DEBUG
	printk("look_dispatch\n");
#endif
	rq = look_elv_rb_find_closest(&ld->root, ld->last_serviced);
	if (rq == NULL) {
		return 0;
	}
	ld->last_serviced = blk_rq_pos(rq);
	elv_dispatch_add_tail(q, rq);
    elv_rb_del(&ld->root, rq);
	return 1;
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

	elv_rb_add(&ld->root, rq);
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
	//INIT_LIST_HEAD(&ld->queue);
	//ld->cur_pos = &ld->queue;
	// Acquire the lock and set the queue's elevator queue
	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);
	ld->last_serviced = 0;
    ld->root = RB_ROOT;
	return 0;
}

static int look_merge(struct request_queue *q, struct request **req,
		struct bio *bio)
{
	struct look_data *ld = q->elevator->elevator_data;
	struct request *rq;
	sector_t position = bio_end_sector(bio);
#if DEBUG
	printk("look_merge\n");
#endif
	rq = elv_rb_find(&ld->root, position);
	if (rq != NULL) {
		*req = rq;
		return ELEVATOR_FRONT_MERGE;
	}
	return ELEVATOR_NO_MERGE;
}

static void look_merged_requests(struct request_queue *q, struct request *rq,
		struct request *next)
{
	struct look_data *ld = q->elevator->elevator_data;
#if DEBUG
	printk("look_merged_requests\n");
#endif
	elv_rb_del(&ld->root, rq);
}

static void look_exit_queue(struct elevator_queue *e)
{
	struct look_data *ld = e->elevator_data;
#if DEBUG
	printk("look_exit_queue\n");
#endif
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
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Look I/O scheduler");
