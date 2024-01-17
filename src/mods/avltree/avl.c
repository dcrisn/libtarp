#include <assert.h>
#include <endian.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <tarp/avl.h>

#include <tarp/common.h>
#include <tarp/error.h>
#include <tarp/math.h>
#include <tarp/staq.h>
#include <tarp/dllist.h>
#include <tarp/log.h>
#include <tarp/bst.h>

 /*
  * General notes on AVL
  * ---------------------
  *  The AVL data structure maintains tree balance invariants so as to eliminate
  *  the possibility of degenerate trees of non-logarithmic shape.
  *
  *  The tree node is augmented with a few bits for tracking the "balance factor"
  *  which is the difference in height between the left and right subtrees.
  *  The absolute height values need not be tracked: since the AVL invariant
  *  entails the 2 child subtrees differing in height by at most 1 (and
  *  performing restorative rotations if this condition is violated), 2 bits
  *  are sufficient for encoding values in the range [-1,1] where:
  *   -1 = left-child heavy (i.e. taller)
  *    0 = child subtrees equal in height
  *    1 = right-child heavy
  *
  * Since the tree is maintained balanced, tree-traversal based operations
  * are bounded to O(log n). Rotations are carried out after changes to
  * the tree structure (insertion, deletion) that break the AVL property.
  * This means insertions and deletions are costlier than lookups (where
  * no change is made to the tree) and therefore the AVL is particularly
  * suited to lookup-focused deployments where deletions and insertions
  * are comparatively rare (much like a hash table).
  * If this is not the case, the Red-black tree alternative may be worth
  * considering as particularly deletions tend to be less expensive there.
  *
  * Notes on the balance factor and height field
  * -------------------------------------------
  * NOTE that while the height need not be explicitly stored in each node,
  * doing so does simplify the logic significantly. With this said, both
  * solutions are provided below. This is for documentation purposes more
  * than anything. The logic that does *not* rely on each node having an
  * explicit height field is compiled by default. The performance is
  * essentially identical with or without the height field but of course
  * without it the memory footprint is reduced.
  *
  * Notes on parent pointers
  * ------------------------
  * Besides the height field, another potential overhead is storing a parent
  * pointer. Just like with the height field, this would simplify the logic
  * significantly. The approach taken here is to use an external stack instead
  * to trace the way from root to the insertion/deletion spot, obviating the
  * need for parent pointers and avoiding the overhead -- at the cost of
  * some added complexity.
  */

#define unbalanced(node)          ((node)->bf < -1 || (node)->bf > 1)
#define off_balance(node)         unbalanced(node)
#define left_heavy(node)          ((node)->bf < 0)
#define right_heavy(node)         ((node)->bf > 0)
#define perfectly_balanced(node)  (node->bf == 0)
#define lopsided(node)            (!perfectly_balanced(node) && !unbalanced(node))

/*
 * 'Heavy' here really means 'taller'/'higher'. But the balance factor visually
 * suggests one side has more 'weight' than the other so the terms used are
 * in keeping with that imagery. */
enum {LEFT_HEAVY=-1, PERFECTLY_BALANCED=0, RIGHT_HEAVY=1};


static inline void initialize_avl_node(struct avlnode *node){
    assert(node);
    node->left = node->right = NULL;
    node->bf = 0;
#ifdef AVL_NODES_WITH_HEIGHT_FIELD
    node->height = 1;
#endif
}

static inline void copy_avlnode_fields(
        struct avlnode *a, struct avlnode *b,
        bool copy_left_child, bool copy_right_child)
{
    if (copy_left_child)  a->left = b->left;
    if (copy_right_child) a->right = b->right;
    a->bf = b->bf;

#ifdef AVL_NODES_WITH_HEIGHT_FIELD
    a->height = b->height;
#endif
}

/*
 * For simplicity, NULL leaves have height 0, childless leaves have
 * height 1. */
static int validate_avl(const struct avlnode *node, bool *priv){
    if (!node) return 0;
    assert(priv);

    unsigned int height_left = validate_avl(node->left, priv);
    unsigned int height_right = validate_avl(node->right, priv);
    unsigned int height = 1 + MAX(height_left, height_right);
    int diff = MAX(height_left, height_right) - MIN(height_left, height_right);

    //info("height %zu", height);

    if (diff > 1 || diff < -1 || node->bf > 1 || node->bf < -1){
        *priv = false;
        debug("Invalid AVL property: bf=%d left.height=%zu right.height=%zu",
                node->bf, height_left, height_right);
    } else {
        *priv = true;
    }

    return height;
}

/*
 * Determine if the tree respects the avl properties. I.e. the balance
 * factor is correct and accurate */
bool isavl(const struct avltree *tree){
    assert(tree);
    if (tree->count == 0) return true;

    bool is = false;
    validate_avl(tree->root, &is);
    return is;
}

static inline bool cache_hit(const struct avltree *tree, const struct avlnode *match)
{
    assert(tree); assert(match);
    if (tree->cached && eq(tree->cached, match, tree->cmp)) return true;
    return false;
}

static inline void clear_cache(struct avltree *tree){
    assert(tree);
    tree->cached = NULL;
}

static inline void update_cache(struct avltree *tree, struct avlnode *node){
    assert(tree);
    tree->cached = node;
}

void Avl_init(struct avltree *tree, avl_comparator cmpf, avlnode_destructor dtor)
{
    assert(tree);
    *tree = AVL_INITIALIZER(cmpf, dtor);
}

struct avltree *Avl_new(avl_comparator cmpf, avlnode_destructor dtor){
    struct avltree *tree = salloc(sizeof(struct avltree), NULL);
    Avl_init(tree, cmpf, dtor);
    return tree;
}

/*
 * avl_* functions are file-static helpers; do_* functions likewise.
 * Avl_* functions are public and declared in the header */
define_bst_waypoint_structure(avl, struct avlnode)

define_bst_height_getter(private, avl, struct avlnode)
define_bst_count_getter(public, Avl, struct avltree)
define_bst_empty_predicate(public, Avl, struct avltree)
define_bst_node_finder(private, avl, struct avltree, struct avlnode)
define_bst_max_getter(public, Avl, struct avltree, struct avlnode)
define_bst_min_getter(public, Avl, struct avltree, struct avlnode)
define_bst_inorder_successor_finder(private, avl, struct avlnode)
define_bst_path_tracer(private, avl, struct avltree, struct avlnode, struct avl_waypoint)
define_bst_cut_down(private, avl, struct avltree, struct avlnode)
define_bst_graphic_dump(private, avl, struct avltree, struct avlnode, avlnode_printer)
define_get_nodes_at_level(private, avl, struct avlnode, struct avlnode_ptr)

define_bst_rotate_right(private, do_avl, struct avlnode) /* do_avl_rotate_{right,left} */
define_bst_rotate_left(private, do_avl, struct avlnode)

static inline void waypoint_staq_dtor(struct staqnode *node){
    salloc(0, get_container(node, struct avl_waypoint, link));
}

/*
 * 0 = empty tree*/
int Avl_height(const struct avltree *tree){
    assert(tree);
    return avl_find_node_height(tree->root);
}

void Avl_clear(struct avltree *tree, bool free_containers){
    assert(tree);

    if (free_containers){
        THROWS_ON(tree->dtor==NULL, ERROR_MISCONFIGURED, "missing destructor");
        avl_cut_down(tree, tree->root);
    }

    *tree = AVL_INITIALIZER(tree->cmp, tree->dtor);
}

void Avl_destroy(struct avltree **tree, bool free_containers){
    assert(tree);
    assert(*tree);
    Avl_clear(*tree, free_containers);
    *tree = NULL;
}

void Avl_print(struct avltree *tree, avlnode_printer pf){
    assert(tree);
    assert(pf);

    struct dllist q = DLLIST_INITIALIZER(NULL);
    avl_dump_tree_graph(tree, tree->root, pf, NULL, &q, 1);
    Dll_clear(&q, false);
}

#ifdef AVL_NODES_WITH_HEIGHT_FIELD
/*
 * The following functions calculate and adjust the balance factor in a given
 * node based on explicitly stored height fields. Rotations, the post-insertion
 * and post-deletion fixup logic here assume there is a height field.
 *
 * Storing the height in each node does simplify the code and logic quite
 * substantially and most AVL implementations, by far, take this approach.
 * However, also subtantial is the overhead, memory-wise, that this incurs.
 * NOTE Storing the height as an int would be more than sufficient and
 * reasonable. Storing it as a size_t for example (assuming that is 8 bytes)
 * is unnecessary since the point of AVL (and other self-balancing tree
 * mechanisms) is to keep the tree from growing too tall, hence assuming a
 * correct implementation, the height will never reach a value too big to
 * fit in an int (assuming sizeof(int) = 4 bytes, for example).
 *
 * This AVL implementation can toggle between using and not using the
 * explicit height field. Again -- the height field makes the code easier
 * and more intuitive to grasp and write, but the overhead is unnecessary.
 * Also, removing the height field exacts no performance penalty. The only
 * downside is more cases that need considering and therefore hairier code.
 * The explicit height-field approach is disabled (compiled out, that is)
 * by default but is still kept for 2 reasons:
 * 1) certain applications may need a height field regardless of AVL.
 * 2) for documentation, future reference and comparison with the more obscure
 * code that relies solely on the balance factor.
 */

static inline void do_update_avl_parameters(struct avlnode *node){
    assert(node);

    /*
     * NULL leaves have height 0; childless leaves have height 1. This
     * simplifies calculating the balance factor ( -- as opposed to having
     * NULL leaves with height -1 instead of 0) */
    struct avlnode *left  = node->left;
    struct avlnode *right = node->right;

    int lh = left ? left->height : 0;
    int rh = right ? right->height : 0;
    node->height = MAX(lh, rh)+1;
    node->bf = rh - lh;
}

// only exists to leave the insert fixup logic unchanged between
// the height-field code (by default compiled out) and the usual
// balance-factor-only reliant code
static inline void update_avl_parameters(
        struct avlnode *node,
        int bfchange,   /* change in the balance factor; -1 or +1 */
        bool isdel      /* if true, called for deletion, else insertion fixup */
        )
{
    /* the height is there as a field, no need for these */
    UNUSED(bfchange);
    UNUSED(isdel);
    do_update_avl_parameters(node);
}

static struct avlnode *avl_rotate_left(
        struct avlnode *parent,
        struct avlnode *child)
{
    struct avlnode *root = do_avl_rotate_left(parent, child);
    do_update_avl_parameters(parent);
    do_update_avl_parameters(child);
    return root;
}

static struct avlnode *avl_rotate_right(
        struct avlnode *parent,
        struct avlnode *child)
{
    struct avlnode *root = do_avl_rotate_right(parent, child);
    do_update_avl_parameters(parent);
    do_update_avl_parameters(child);
    return root;
}

/* always return false; the signature is like this only to leave
 * the fixup logic unchanged between the explicit height-field
 * code (compiled out by default) and the balance-factor-only reliant
 * code */
static inline bool fix_imbalance(
        struct avlnode **upper, /* grandparent or root */
        struct avlnode *parent,
        struct avlnode *child)
{
    assert(upper);
    assert(parent);
    assert(child);

    struct avlnode *top = NULL;

    if (left_heavy(parent)){
        if (right_heavy(child)){  /* LR */
            parent->left = avl_rotate_left(child, child->right);
            top          = avl_rotate_right(parent, parent->left);
        } else {   /* LL */
            top          = avl_rotate_right(parent, parent->left);
        }
    }
    else {
        assert(right_heavy(parent));
        if (left_heavy(child)){  /* RL */
            parent->right    = avl_rotate_right(child, child->left);
            top              = avl_rotate_left(parent, parent->right);
        } else {   /* RR */
            top              = avl_rotate_left(parent, parent->right);
        }
    }

    *upper = top;
    return false;  /* (1) */
}

#else /* !AVL_NODES_WITH_HEIGHT_FIELD ; rely solely on balance factor */

/*
 * Perform a double left-right rotation to make grandchild the root
 * of a tree with parent as the new right child and child as the new
 * left child.
 * The balance factors of the nodes are updated.
 *
 *             p                  _gc_
 *           / \                /     \
 *         c   T3              c      p
 *       / \         =>      /  \    / \
 *     t0  gc               T0 T1   T2 T3
 *        / \
 *       T1 T2
 *
 * Cases:
 * 1) LEFT_HEAVY:          T1 > T2
 * 2) RIGHT_HEAVY:         T1 < T2
 * 3) PERFECTLY_BALANCED:  T1 == T2
 */
static struct avlnode *avl_left_right(
        struct avlnode *parent,
        struct avlnode *child,
        struct avlnode *grandchild)
{
    assert(parent);
    assert(child);

    /* adjust balance factors to what they will be *after* the rotations */
    switch(grandchild->bf){
    case LEFT_HEAVY:
        parent->bf = 1;
        child->bf = 0;
        break;
    case RIGHT_HEAVY:
        parent->bf = 0;
        child->bf = -1;
        break;
    case PERFECTLY_BALANCED:
        parent->bf = child->bf = 0;
        break;
    }
    grandchild->bf = 0;

    parent->left = do_avl_rotate_left(child, child->right);
    return do_avl_rotate_right(parent, parent->left);
}

/*
 * Perform a double right-left rotation to make grandchild the root
 * of a tree with parent as the new left child and child as the new
 * right child.
 * The balance factors of the nodes are updated.
 *
 *            p                    _gc_
 *          /  \                 /     \
 *        T0    c       =>      p      c
 *            /  \             / \    / \
 *          gc    T3          T0 T1  T2 T3
 *        /  \
 *       T1   T2
 *
 * Cases:
 * 1) LEFT_HEAVY:         T1 > T2
 * 2) RIGHT_HEAVY:        T1 < T2
 * 3) PERFECTLY_BALANCED: T1 == T2
 */
static struct avlnode *avl_right_left(
        struct avlnode *parent,
        struct avlnode *child,
        struct avlnode *grandchild)
{
    assert(parent);
    assert(child);

    /* adjust balance factors to what they will be *after* the rotations */
    switch(grandchild->bf){
    case LEFT_HEAVY:
        parent->bf = 0;
        child->bf = 1;
        break;
    case RIGHT_HEAVY:
        parent->bf = -1;
        child->bf = 0;
        break;
    case PERFECTLY_BALANCED:
        parent->bf = child->bf = 0;
        break;
    }
    grandchild->bf = 0;

    parent->right = do_avl_rotate_right(child, child->left);
    return do_avl_rotate_left(parent, parent->right);
}

/*
 * Perform a single left rotation to move child above parent.
 * Balance factors are updated.
 *
 * Notice that when fixing up the tree after a deletion, one of the conditions
 * that can arise (which never happens with insertion) is that the child has
 * a balance factor of 0. If we get this, we have to adjust the balance
 * factors as shown below and we can stop the deletion fixup loop, since
 * the tree has become 'lopsided' and the height does not change in this subtree
 * and by extension in the whole tree.
 */
static struct avlnode *avl_left(
        struct avlnode *parent,
        struct avlnode *child,
        bool *del_fixup_done)
{
    assert(parent);
    assert(child);
    assert(del_fixup_done);

    /* adjust balance factors to what they will be *after* the rotations */
    if (lopsided(child)){ /* always true when inserting */
        parent->bf = child->bf = 0;
    } else {             /* sometimes when deleting, never when inserting */
        *del_fixup_done = true;
        parent->bf = 1;
        child->bf = -1;
    }
    return do_avl_rotate_left(parent, child);
}

/*
 * Perform a single left rotation to move child above parent.
 * Balance factors are updated. See avl_left fmi. */
static struct avlnode *avl_right(
        struct avlnode *parent,
        struct avlnode *child,
        bool *del_fixup_done)
{
    assert(parent);
    assert(child);
    assert(del_fixup_done);

    /* adjust balance factors to what they will be *after* the rotations */
    if (lopsided(child)){ /* always true when inserting */
        parent->bf = child->bf = 0;
    } else {             /* sometimes when deleting, never when inserting */
        *del_fixup_done = true;
        parent->bf = -1;
        child->bf = 1;
    }
    return do_avl_rotate_right(parent, child);
}

/*
 * This function updates the balance factor of the given node based on the
 * latest deletion/insertion. This can cause a tilt, balance off an existing
 * tilt, or create an imbalance that needs rotations. If rotations are
 * performed, the balance factors affected are updated in the functions
 * performing the rotation(s).
 *
 * NOTE
 * bfchange is simply the 'dir' field from the path trace. It will be either
 * LEFT or RIGHT. Since LEFT is -1 and RIGHT is +1, these can be added to (when
 * updating after insert) or subtracted from (when updating after delete) the
 * balance factor of the node to update it.
 *
 * Specifically:
 *  - if isdel=false, this is a bf update after an insert. The node has been
 *  inserted somewhere in the right (dir=1/RIGHT) or left (dir=-1/LEFT) subtree
 *  of the given node. Therefore the balance factor for node increases by
 *  1 or -1 i.e. dir RIGHT or dir LEFT.
 * - if isdel=true, this is a bf update after a deletion. The node has been
 *   removed from somewhere in the right (dir=1/RIGHT) or left (dir=-1/LEFT)
 *   subtree of the given node. Therefore the balance factor for node increases
 *   by 1 *the other way*. I.e. it decrases by 1 (dir=RIGHT) or by -1 (
 *   dir=LEFT). E.g. if it is -1 and the deletion happened in the left (dir=-1)
 *   subtree, then bf decreases by -1 to become 0.
 */
static inline void update_avl_parameters(
        struct avlnode *node,
        int bfchange,   /* change in the balance factor; -1 or +1 */
        bool isdel      /* if true, called for deletion, else insertion fixup */
        )
{
    assert(node);
    assert(bfchange == -1 || bfchange == 1);

    if (isdel) node->bf -= bfchange;
    else       node->bf += bfchange;
}

/*
 * @return
 * If true, the fixup loop shoud stop; there is a special case for deletion
 * where this can happen; for insertion the return value is always false
 * and can be ignored */
static inline bool fix_imbalance(
        struct avlnode **upper, /* grandparent or root */
        struct avlnode *parent,
        struct avlnode *child)
{
    assert(upper);
    assert(parent);
    assert(child);

    struct avlnode *top = NULL;
    bool must_stop = false;

    if (left_heavy(parent)){
        if (right_heavy(child)){  /* LR */
            top = avl_left_right(parent, child, child->right);
        } else {
            top = avl_right(parent, child, &must_stop);  /* LL */
        }
    } else {
        if (left_heavy(child)){  /* RL */
            top = avl_right_left(parent, child, child->left);
        } else {
            top = avl_left(parent, child, &must_stop);  /* RR */
        }
    }

    *upper = top;
    return must_stop;
}
#endif  /* !AVL_NODES_WITH_HEIGHT_FIELD */

/*
 * NOTE
 * While working our way back up the tree after an insertion, if:
 *
 * (1) the current node is perfectly balanced (*after* initially updating
 * its balance factor), then the height of the subtree rooted at this node
 * (and by extension the height of the whole tree) does not change and hence
 * we can return immediately.
 *
 * (2) the current node is off balance, then the violation must be corrected
 * through a rotation (single or double). The routines will update the
 * balance factor(s) as well as appropriate. When inserting, a *single*
 * rotation globally repairs the tree so we're all done here.
 *
 * -----------------------------
 *
 * (3) It's possible we either reach the end of the tree without finding
 * any nodes off balance, in which case return, or we've broken out
 * of the loop early to deal with the only possible unbalanced node,
 * as explained.
 */
void avl_insert_fixup(struct avltree *tree, struct staq *path){
    assert(tree);
    assert(path);

    struct avl_waypoint *upper;
    struct avlnode *parent, *child;
    enum pathTraceDirection dir;

    while ((upper = Staq_pop(path, struct avl_waypoint, link))){
        parent = upper->ptr;
        dir = upper->dir;
        assert(parent);

        update_avl_parameters(parent, dir, false);
        salloc(0, upper);

        if (perfectly_balanced(parent)) return;   // (1)

        if (off_balance(parent)) break;   // (2)
    }

    /* either no node off balance or reached the only possible unbalanced
     * node (3) */
    if (!upper){
        //debug("upper pointer NULL, returning, stack done");
        return;
    }

    child = (dir == LEFT) ? parent->left : parent->right;
    struct avlnode **ref = &tree->root;
    if ( (upper = Staq_top(path, struct avl_waypoint, link)) ) {
        ref = (upper->dir == LEFT)
            ? &upper->ptr->left
            : &upper->ptr->right;
    }
    fix_imbalance(ref, parent, child);
}

/*
 * NOTE
 * Like for the insertion fixup logic, there are certain conditions that allow
 * the deletion fixup loop to stop early.
 *
 * (1) If the node is tilted one way or the other, then it means the height
 * of the subtree (and by extension the height of the whole tree) does not
 * change, and we can return early.
 *
 * (2) If the node is perfectly balanced, then the height has in fact
 * decreased and the checks and updates must continue back up the tree.
 *
 * When a node is found to be off balance, rotations are performed (and the
 * balance factors are of course adjusted). Unlike in the case of insertion,
 * however, one rotation may not be enough and the checks must always continue
 * up the treee (until the condition indicated at (1) is otherwise encountered).
 * The exception (3) is a special case that occurs when deleting a node, as
 * explained in the comment above avl_left. If this has occured,
 * delete_fixup returns true and this indicates the loop can be exited.
 */
void avl_delete_fixup(struct avltree *tree, struct staq *path){
    assert(tree);
    assert(path);

    struct avl_waypoint *upper;
    struct avlnode *parent, *child;
    int dir;

#ifdef MAGIC_CONTAINERS
    assert(path->cast);
#endif
    while ((upper = Staq_pop(path, struct avl_waypoint, link))){
        parent = upper->ptr;
        assert(parent);
        dir = upper->dir;
        salloc(0, upper);

        update_avl_parameters(parent, dir, true);

        if (lopsided(parent)) return;  /* (1) */

        if (!off_balance(parent)){  /* (2) */
            assert(perfectly_balanced(parent));
            continue;
        }

        assert(dir == -1 || dir == 1);
        child = (dir == 1) ? parent->left : parent->right; /* pass sibling */

        struct avlnode **ref = &tree->root;
        if ( (upper = Staq_top(path, struct avl_waypoint, link)) ) {
            assert(upper->dir == -1 || upper->dir == 1);
            ref = (upper->dir == LEFT)
                ? &upper->ptr->left
                : &upper->ptr->right;
        }

        if (fix_imbalance(ref, parent, child)) return; /* (3) */
    }
}

/*
 * Insert the given node in the tree if it's not a duplicate.
 *
 * Path must be a pointer to an initialized staq handle; this will be
 * populated, stack-like, with struct avl-waypoint entries. The caller
 * is responsible for destroying the path and popping any waypoint entries
 * still on it.
 *
 * NOTES
 *
 * (1) It takes a 2-edge long path for the AVL property to be broken. IOW
 * since after an insertion, the immediate parent p can be tilted, but not
 * outright thrown off balance (see (2)), the actual imbalance (resulting
 * from the tilt) can only occur at the grandparent, or even further back up
 * the tree (if at all). So to immediate parent can be discarded from the stack,
 * and the fixup routine can start at the grandparent. This is not done however
 * for consistency so that all balance factor updates happen in the fixup
 * function.
 */
static bool avl_try_insert(
        struct staq *path,
        struct avltree *tree,
        struct avlnode *node)
{
    assert(tree);
    assert(node);
    assert(path);

    initialize_avl_node(node);

    if (!tree->root){
        assert(tree->count == 0);
        tree->root = node;
        tree->count++;
        return true;
    }

    avl_trace_path_to_node(tree, path, NULL, node);
    struct avl_waypoint *parent = Staq_top(path, struct avl_waypoint, link); /* (1) */
    assert(parent);
    if (!parent) return false;

    if (parent->dir == 0){ /* node already exists */
        //debug("duplicate");
        return false;
    }

    assert(parent->dir == LEFT || parent->dir == RIGHT);
    struct avlnode *p = parent->ptr;
    assert(p);

    if (parent->dir == LEFT) p->left = node;
    else                     p->right = node;
    tree->count++;

    avl_insert_fixup(tree, path);
    return true;
}

bool Avl_insert_node(struct avltree *tree, struct avlnode *node){
    struct staq path = STAQ_INITIALIZER(waypoint_staq_dtor);

    /* duplicate => insertion *will* fail */
    if (cache_hit(tree, node)) return false;

    bool inserted = avl_try_insert(&path, tree, node);
    Staq_clear(&path, true);
    return inserted;
}

/*
 * If we get a cache hit, the node must positively exist in the tree
 * (because deletions clear the cache as appropriate) and therefore
 * the insertion will fail.
 *
 * NOTE the cache is only updated on lookups, not insertions. Therefore
 * if the insertion succeeds, semantically this is an insertion, not a lookup,
 * and the cache stays the way it is. Conversely if the insertion fails, the
 * duplicate node is returned and so it can be considered a lookup.
 */
struct avlnode *Avl_find_or_insert_node(
        struct avltree *tree, struct avlnode *node)
{
    if (cache_hit(tree, node)) return tree->cached; /* (1) */

    struct avlnode *entry = NULL;

    struct staq path = STAQ_INITIALIZER(waypoint_staq_dtor);
    bool inserted = avl_try_insert(&path, tree, node);

    if (!inserted){
        struct avl_waypoint *duplicate = Staq_top(&path, struct avl_waypoint, link);
        assert(duplicate);
        assert(duplicate->ptr);
        if (duplicate && duplicate->ptr){
            entry = duplicate->ptr;
            update_cache(tree, entry);
        }
    }

    Staq_clear(&path, true);
    return entry;
}

struct avlnode *Avl_find_node(
        struct avltree *tree,
        const struct avlnode *key)
{
    assert(tree);

    if (cache_hit(tree, key)) return tree->cached;

    struct avlnode *found = avl_find_node(tree, key);

    /* Only update the cache on successful lookups */
    if (found) tree->cached = found;

    return found;
}

bool Avl_has_node(const struct avltree *tree, const struct avlnode *key){
    return (Avl_find_node((struct avltree *)tree, key) != NULL);
}

/*
 * Deletion follows the usual tree-deletion algorithm. Things to note are that
 * a path through the tree is traced and the nodes along the path are returned
 * on a stack. This is one alternative to using space-costly parent pointers.
 *
 * If we have the simple case of a node with an only child, then our trace
 * and deletion are complete. But otoh if we have a node with two children,
 * then we have to replace it with its in-order successor (or predecessor),
 * and our effective deletion spot has changed. Therefore the path must be
 * extended. Notice (1) the node we thought was going to be deleted is pushed
 * back onto the stack as it now represents a valid waypoint (the actual node
 * that is going to be deleted is the former in-order successor);
 * and the rest of the path to the new deletion spot is traced (2) and
 * this stack of waypoints is placed on top of the original one to
 * extend/complete it. */
bool Avl_delete_node(
        struct avltree *tree,
        struct avlnode *k,
        bool free_container)
{
    assert(tree);
    assert(k);
    struct avlnode *node = NULL;
    void *node_tofree = NULL;

    if (tree->count == 0)
        return false;

    struct staq path = STAQ_INITIALIZER(waypoint_staq_dtor);
    struct staq replacement_path = STAQ_INITIALIZER(waypoint_staq_dtor);

    avl_trace_path_to_node(tree, &path, NULL, k);
    if (Staq_count(&path) == 0) return false; /* 404 */

    struct avl_waypoint *cur_ptr    = Staq_pop(&path, struct avl_waypoint, link);
    struct avl_waypoint *parent_ptr = Staq_top(&path, struct avl_waypoint, link);

    assert(cur_ptr && cur_ptr->ptr);
    k = cur_ptr->ptr;
    assert(k);
    node_tofree = k;

    if (left_child_only(k) || right_child_only(k)){
        node = left_child_only(k) ? k->left : k->right;
    }

    else if (both_children(k)){
        cur_ptr->dir = 1;
        Staq_push(&path, cur_ptr, link); /* (1) */
        struct avlnode *suc = avl_find_inorder_successor(k);
        assert(suc);

        if (suc != k->right){
            avl_trace_path_to_node(tree, &replacement_path, k->right, suc); /* (2) */
            Staq_remove(&replacement_path, true); /* pop suc pointer */
            struct avl_waypoint *suc_parent_ptr = \
                        Staq_top(&replacement_path, struct avl_waypoint, link);
            assert(suc_parent_ptr && suc_parent_ptr->ptr);
            if(suc_parent_ptr && suc_parent_ptr->ptr){
                suc_parent_ptr->ptr->left = suc->right;
            }

            Staq_join(&replacement_path, &path);
            Staq_swap(&path, &replacement_path);
        }

        copy_avlnode_fields(suc, k, true, (suc != k->right));
        node = suc;
        cur_ptr->ptr = suc;
        cur_ptr = NULL;  /* to prevent freeing the pointer below */
    }

    /* link to rest of the tree */
    if (! (parent_ptr && parent_ptr->ptr)) tree->root = node;
    else if (parent_ptr->dir == -1) parent_ptr->ptr->left  = node;
    else                            parent_ptr->ptr->right = node;

    if (tree->root) avl_delete_fixup(tree, &path);

    salloc(0, cur_ptr);
    if (free_container){
        THROWS_ON(tree->dtor==NULL, ERROR_MISCONFIGURED, "missing destructor");
        tree->dtor(node_tofree);
    }

    Staq_clear(&path, true);
    clear_cache(tree);

    tree->count--;
    return true;
}

/*
 * Find the k-th element when performing in-order traversal.
 * K is 1 based such that k=1 is min(tree).
 * When the node is found, the recursion unrolls, stopping the traversal.
 *
 * --> k
 * which element to find (the kth when performing inorder traversal)
 *
 * --> n
 * the current number of the node. 0 must be passed when calling this function.
 *
 * --> backward
 * If true, the inorder traversal happens in reverse. That is, instead of
 * left-parent-right, it's right-parent-left. This is useful when searching
 * for a function closer to the max than to the min.
 *
 * --> found
 *  Out-param. Must be non-NULL. Will be set to point to the kth node found.
 *  NULL if no node is found (because k > count(tree)).
 */
static inline size_t find_kth_inorder(
        const struct avlnode *node,
        size_t k, size_t n,
        bool backward,
        struct avlnode **found)
{
    struct avlnode *child = NULL;

    if (!node) return n;

    child = backward ? node->right : node->left;
    n = find_kth_inorder(child, k, n, backward, found);

    /* found, return early, dont't even bother updating */
    if (n == k) return n;

    /* current node is the nth node when traversing in order ? */
    if (++n == k){
        *found = (struct avlnode*)node;
        return n;
    }

    /* else keep looking */
    child = backward ? node->left : node->right;
    return find_kth_inorder(child, k, n, backward, found);
}

/*
 * If n is the number of nodes in the tree, then note:
 *  - the kth smallest element is the n-(k-1)th greatest element
 *  - the kth greatest element is the n-(k-1)th smallest element
 * Therefore finding the k-th smallest/greatest can be cast as the same
 * problem.
 *
 * NOTE that for small k it is most efficient to start from the closer end
 * (the min end OR the max end, that is, whichever one is closer).
 * This is so that the portion of the tree that needs to be traversed is
 * minimized. That is, if you want the 3rd smallest element, you need to
 * start from the smallest element and work backward. Similarly, if you
 * want the 3rd greatest element, you want to start from the greatest element
 * and work backward. Therefore the algorithm tries to figure out the closest
 * extremum, and start searching from there as the recursion unrolls.
 * This intuitively means that the performance for finding both the kth min
 * and max degrades the farther k is from min/max and the closer it is to the
 * middlle. While for small k that can be placed close to either the min or max
 * end the performance will be very good -- near constant.
 *
 * The algorithm is essentially equivalent to linear search (as opposed to
 * binary search, sadly) in a sorted list and therefore the complexity is O(n).
 * For binary search (and O(log n) complexity), we would need to augment to
 * avlnode structs with another field: the count of the subtree rooted at that
 * node. This would make it possible to rule out traversal of any subtree of
 * unsuitable size. However, it's desirable to keep the basic core data
 * structure as lean as possible so under these circumstances this algorithm is
 * fine -- as long as k is kept reasonably small (close to min/max).
 * Alternatively, just use a min/max heap since that's their whole point.
 */
struct avlnode *Avl_get_kth_min(const struct avltree *tree, size_t k){
    assert(tree);
    assert(k>0);

    if (k > tree->count) return NULL;

    struct avlnode *found = NULL;
    size_t half = tree->count/2;
    find_kth_inorder(tree->root,
            (k > half) ? tree->count - (k-1) : k,
            0,
            (k > half) ? true : false,
            &found);

    return found;
}

struct avlnode *Avl_get_kth_max(const struct avltree *tree, size_t k){
    assert(tree);
    assert(k>0);

    if (k > tree->count) return NULL;

    return Avl_get_kth_min(tree, tree->count - (k-1));
}

size_t Avl_num_levels(const struct avltree *tree){
    return Avl_height(tree) + 1;
}

void Avl_get_nodes_at_level(
        const struct avltree *tree,
        size_t level,
        struct staq *queue)
{
    assert(queue);
    assert(tree);

    *queue = STAQ_INITIALIZER(NULL);
    avl_get_nodes_at_level(tree->root, level, queue, NULL);
}

size_t AVL_count_nodes_at_level(const struct avltree *tree, size_t level){
    assert(tree);
    size_t count = 0;
    avl_get_nodes_at_level(tree->root, level, NULL, &count);
    return count;
}

#if 0
TODO: implement set operations e.g.
struct avltree *Avl_join(struct avltree *a, struct avltree *b);
struct avltree *Avl_intersect(struct avltree *a, struct avltree *b);
struct avltree *Avl_difference(struct avltree *a, struct avltree *b);
#endif


