#include <assert.h>
#include <tarp/bst.h>
#include <tarp/staq.h>
#include <tarp/common.h>
#include <tarp/dllist.h>
#include <tarp/math.h>

/* define <struct bst_waypoint> with a 'ptr' pointer field to
 * a <struct bstnode *>. This structure can be linked in a staq */
define_bst_waypoint_structure(bst, struct bstnode)

/* define <struct bstnode_ptr> with a 'ptr' pointer field to
 * a <struct bstnode *> */
define_bstnode_ptr_wrapper_structure(bstnode, struct bstnode)

void Bst_init(struct bst *tree, bst_comparator cmpf, bstnode_destructor dtor){
    assert(tree);
    *tree = BST_INITIALIZER(cmpf, dtor);
}

define_bst_count_getter(public, Bst, struct bst)
define_bst_empty_predicate(public, Bst, struct bst)
define_bst_node_finder(public, Bst, struct bst, struct bstnode)
define_bst_max_getter(public, Bst, struct bst, struct bstnode)
define_bst_min_getter(public, Bst, struct bst, struct bstnode)
define_bst_boolean_lookup(public, Bst, struct bst, struct bstnode)
define_bst_inorder_successor_finder(public, Bst, struct bstnode)
define_bst_path_tracer(public, Bst, struct bst, struct bstnode, struct bst_waypoint)
define_bst_cut_down(public, Bst, struct bst, struct bstnode)
define_bst_rotate_right(public, Bst, struct bstnode)
define_bst_rotate_left(public, Bst, struct bstnode)
define_get_nodes_at_level(public, Bst, struct bstnode, struct bstnode_ptr)
define_bst_height_getter(public, Bst, struct bstnode)

/*
 * IMPLEMENTATION NOTES
 * =====================
 *
 * Dangers of recursion depth
 * ---------------------------
 * The function is recursive. This means there's a risk of exhausing the stack
 * if the tree is indeed degenerate and very big. This is tantamount to a
 * decrease-by-one recursion on a linked list, which is at elevated risk of
 * callstack overflow.
 * If this is a concern, it should be trivial to convert the function to an
 * iterative version by using an external callstack instead (#TODO).
 *
 * Implementation
 * ----------------
 * The underlying idea is that once you've printed something to stdout you can't
 * go back and print something on previous upper lines or to the left on the
 * current line (that is, without using ANSI terminal control sequences).
 * This then means:
 *  - if we are to print a tree as growing left to right (root on the left,
 * nodes on the right), we must print, recursively, right child, subtree root,
 * left child.
 *
 * Each tree node is printed on its own separate line. The problem is, what does
 * the e.g. rightmost child of a 3-level tree print, what does its parent
 * print, and what does its left sibling print? How do the lines differ?
 * The line must be constructed piecemeal such that lower nodes down the tree
 * take into account nodes higher upper:
 *  - the current node (unless it is root), has a parent, and presumably a left
 * and right child. The parent was (or *will be* if the current node is a right
 * child) printed a little to the left on its line compared to the position
 * of the current node. So in place of the slot/gap where the parent was
 * printed on its own line, we insert padding on the line of the current node,
 * and *then* we print a marker and the strig representation of the current
 * node. This is done recursively such that child nodes are offset futher
 * to the right compared to their parents. This means the line that a node
 * will print is the same line as its parent printed, with the difference
 * that in place of the string representation (and marker) of the parent,
 * padding is in that gap.
 * So the algorithm goes like this:
 *   - node gets line from its parent as a stack of string parts, to be printed
 * bottom to top
 *   - node fills the gap for its string representation with filler (whitespace
 * padding, indentatation marker etc ) by pushing that as a string part onto
 * the stack. It passes this to its right child (which recursively carries
 * out the same steps).
 *  - node pops the right child filler from the stack and prints the line
 * (consisting of each string part on the stack, bottom to top), followed
 * by its own string representation.
 * - node pushes back onto the stack filler for the *left* child, and passes
 * the string-part stack to it as it did for the right child.
 *
 * Indentation/level markers
 * ----------------------------
 * Given two sibling nodes a and b, where a is the left child of its parent
 * and b is the right child of its parent, notice a may be separated from b
 * (and from its parent) visually by many lines due to a large a.right subtree
 * or/and a large b.left subtree, respectively.
 * To visually connect a and b, respectively, to their parent even in this case
 * the following is done (recursively). When the parrent pushes the child
 * filler onto the stack before passing the stack to the child, the filler must
 * include a vertical indentation marker at its beginning to visually join
 * the child to its parent. This is referred to as the 'level trace' in the code
 * below. Since lines as explained are constructed recursively a given line
 * may have many level trace markers to indicate the connection of various
 * tree levels.
 *
 * Notice the following:
 *  - if the node is a left child of its parent, the level trace is only set
 *  for its *right* child. Since the node can only be separated from its parent
 *  by the width of its right subtree.
 *  - if the node is a right child of its parent, the level trace is only set
 *  for its *left* child. Since the node can only be separated from its parent
 *  by the width its left subtree.
 */
define_bst_graphic_dump(public, Bst, struct bst, struct bstnode, bstnode_printer)



// TODO
//void Bst_print(struct bst *tree, bool graphed, bstnode_printer pf);
