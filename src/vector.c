#include "ether/ether.h"

#include <stdlib.h>
#include <math.h>
#include <assert.h>


ETH_MODULE("ether:vector")


#define NODE_SIZE_LOG2 5
#define NODE_SIZE (1 << NODE_SIZE_LOG2)


////////////////////////////////////////////////////////////////////////////////
//                            Auxilary Tree
////////////////////////////////////////////////////////////////////////////////
typedef struct vector_node vector_node;
struct vector_node {
  union {
    vector_node *subnodes[NODE_SIZE];
    eth_t leafs[NODE_SIZE];
  };
  int rc;
};

// TODO: use uniform allocator
static inline vector_node*
create_node(void)
{
  return calloc(1, sizeof(vector_node));
}

static inline void
dec_node(vector_node *node)
{
  node->rc -= 1;
}

static inline void
unref_node(vector_node *node, int depth)
{
  if (--node->rc == 0)
  {
    if (depth == 1)
    {
      for (int i = 0; i < NODE_SIZE; ++i)
      {
        if (node->leafs[i])
          eth_unref(node->leafs[i]);
        else
          break;
      }
    }
    else
    {
      for (int i = 0; i < NODE_SIZE; ++i)
      {
        if (node->subnodes[i])
          unref_node(node->subnodes[i], depth - 1);
        else
          break;
      }
    }
    free(node);
  }
}

static inline void
ref_node(vector_node *node)
{
  node->rc += 1;
}

static inline vector_node*
copy_leaf(const vector_node *restrict node)
{
  vector_node *newnode = calloc(1, sizeof(vector_node));
  for (int i = 0; (i < NODE_SIZE) & (node->leafs[i] != NULL); ++i)
    eth_ref(newnode->leafs[i] = node->leafs[i]);
  return newnode;
}

static inline vector_node*
copy_branch(const vector_node *restrict node)
{
  vector_node *newnode = calloc(1, sizeof(vector_node));
  for (int i = 0; (i < NODE_SIZE) & (node->subnodes[i] != NULL); ++i)
    ref_node(newnode->subnodes[i] = node->subnodes[i]);
  return newnode;
}

typedef struct vector_tree {
  int size;
  int depth;
  vector_node *root;
} vector_tree;

static void
init_tree(vector_tree *tree)
{
  tree->root = NULL;
  tree->size = 0;
  tree->depth = 0;
}

static void
destroy_tree(vector_tree *tree)
{
  if (tree->root)
    unref_node(tree->root, tree->depth);
}

static vector_node*
find_node(const vector_tree *tree, int *restrict k)
{
  vector_node *curr = tree->root;
  int log2cap = NODE_SIZE_LOG2 * tree->depth;
  int isub = -1;
  for (int i = tree->depth; i > 1; --i)
  {
    log2cap -= NODE_SIZE_LOG2;
    isub = *k >> log2cap;
    *k -= isub << log2cap;
    curr = curr->subnodes[isub];
  }
  eth_debug("isub: %d", isub);
  return curr;
}

static void
append_node(vector_node *root, int k, int depth, int log2cap, vector_node *node);

static vector_node*
append_node_pers(const vector_node *root, int k, int depth, int log2cap,
    vector_node *node);

// TODO: optimize path throug unintialized branch
static void
append_node(vector_node *root, int k, int depth, int log2cap, vector_node *node)
{
  log2cap -= NODE_SIZE_LOG2;
  int isub = k >> log2cap;
  k -= isub << log2cap;

  if (root->subnodes[isub] == NULL)
  // unintialized branch
  {
    if (depth == 2)
    // reached level with leaf-nodes (thus insert supplied node)
    {
      ref_node(root->subnodes[isub] = node);
      return;
    }
    else
    // init branch node
      ref_node(root->subnodes[isub] = create_node());
  }
  else if (root->subnodes[isub]->rc > 1)
  {
    vector_node *newbranch =
      append_node_pers(root->subnodes[isub], k, depth - 1, log2cap, node);
    dec_node(root->subnodes[isub]);
    ref_node(root->subnodes[isub] = newbranch);
  }

  append_node(root->subnodes[isub], k, depth - 1, log2cap, node);
}

static vector_node*
append_node_pers(const vector_node *root, int k, int depth, int log2cap,
    vector_node *node)
{
  log2cap -= NODE_SIZE_LOG2;
  int isub = k >> log2cap;
  k -= isub << log2cap;

  if (depth == 1)
  {
    return node;
  }
  else
  {
    vector_node *newroot = copy_branch(root);
    if (newroot->subnodes[isub] == NULL)
    // unintialized branch
    {
      ref_node(newroot->subnodes[isub] = create_node());
      // can do mutable update now
      append_node(newroot->subnodes[isub], k, depth - 1, log2cap, node);
      return newroot;
    }
    else
    {
      vector_node *newbranch =
        append_node_pers(newroot->subnodes[isub], k, depth - 1, log2cap, node);
      dec_node(newroot->subnodes[isub]);
      ref_node(newroot->subnodes[isub] = newbranch);
      return newroot;
    }
  }
}


static void
push_node(vector_tree *restrict tree, vector_node *node)
{
  if (tree->depth == 0)
  // - - - - - -
  // Empty tree
  // - - - - - -
  {
    tree->root = node;
    tree->size += NODE_SIZE;
    tree->depth = 1;
  }
  else
  // - - - - - - - -
  // Non-empty tree
  // - - - - - - - -
  {
    int log2cap = NODE_SIZE_LOG2 * tree->depth;
    if (tree->size >= (1 << log2cap))
    // - - - - - - - - - -
    // New level required
    // - - - - - - - - - -
    {
      vector_node *newroot = create_node();
      newroot->subnodes[0] = tree->root;
      ref_node(tree->root = newroot);
      tree->depth += 1;
      push_node(tree, node);
    }
    else
    // - - - - - - -
    // Append node
    // - - - - - - -
    {
      if (tree->root->rc > 1)
      {
        vector_node *newroot =
          append_node_pers(tree->root, tree->size, tree->depth, log2cap, node);
        dec_node(tree->root);
        ref_node(tree->root = newroot);
      }
      else
        append_node(tree->root, tree->size, tree->depth, log2cap, node);
      tree->size += NODE_SIZE;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
//                              Vector
////////////////////////////////////////////////////////////////////////////////
typedef struct {
  eth_header header;
  vector_tree tree;
  int tailsize;
  vector_node *tail;
} vector;
#define VECTOR(x) ((vector*)(x))

eth_type *eth_vector_type;

static void
destroy_vector(eth_type *type, eth_t x)
{
  vector *vec = VECTOR(x);
  destroy_tree(&vec->tree);
  unref_node(vec->tail, 1);
  free(vec);
}

void
_eth_init_vector_type(void)
{
  eth_vector_type = eth_create_type("vector");
  eth_vector_type->destroy = destroy_vector;
}

static vector*
create_vector(void)
{
  vector *vec = malloc(sizeof(vector));
  eth_init_header(vec, eth_vector_type);
  init_tree(&vec->tree);
  vec->tailsize = 0;
  ref_node(vec->tail = create_node());
  return vec;
}

static vector*
copy_vector(vector *vec)
{
  vector *newvec = malloc(sizeof(vector));
  eth_init_header(newvec, eth_vector_type);
  // copy tree
  newvec->tree = vec->tree;
  if (newvec->tree.root)
    ref_node(newvec->tree.root);
  // copy tail
  newvec->tailsize = vec->tailsize;
  ref_node(newvec->tail = vec->tail);

  return newvec;
}

static void
push(vector *vec, eth_t x)
{
  if (vec->tailsize == NODE_SIZE)
  {
    push_node(&vec->tree, vec->tail);
    dec_node(vec->tail);
    ref_node(vec->tail = create_node());
    vec->tailsize = 0;
  }
  else if (vec->tail->rc > 1)
  {
    vector_node *newtail = copy_leaf(vec->tail);
    ref_node(newtail);
    dec_node(vec->tail);
    vec->tail = newtail;
  }
  eth_ref(vec->tail->leafs[vec->tailsize++] = x);
}

static eth_t
front(vector *vec)
{
  if (vec->tree.size > 0)
  {
    int k = 0;
    vector_node *node = find_node(&vec->tree, &k);
    return node->leafs[k];
  }
  else
    return vec->tail->leafs[0];
}

static eth_t
get(vector *vec, int k)
{
  if (k >= vec->tree.size)
    return vec->tail->leafs[k - vec->tree.size];
  else
  {
    vector_node *node = find_node(&vec->tree, &k);
    return node->leafs[k];
  }
}

eth_t
eth_create_vector(void)
{
  return ETH(create_vector());
}

int
eth_vec_len(eth_t v)
{
  return VECTOR(v)->tree.size + VECTOR(v)->tailsize;
}

void __attribute__((flatten))
eth_push_mut(eth_t v, eth_t x)
{
  push(VECTOR(v), x);
}

eth_t __attribute__((flatten))
eth_push_pers(eth_t v, eth_t x)
{
  vector *vec = copy_vector(VECTOR(v));
  push(vec, x);
  return ETH(vec);
}

eth_t __attribute__((flatten))
eth_front(eth_t v)
{
  return front(VECTOR(v));
}

eth_t __attribute__((flatten))
eth_back(eth_t v)
{
  return VECTOR(v)->tail->leafs[VECTOR(v)->tailsize - 1];
}

eth_t __attribute__((flatten))
eth_vec_get(eth_t v, int k)
{
  return get(VECTOR(v), k);
}

////////////////////////////////////////////////////////////////////////////////
//                               Tests
////////////////////////////////////////////////////////////////////////////////
static void
print_nodes_at(const vector_node *node, int depth, FILE *out, int isleafs)
{
  if (node == NULL)
  // Unintialized branch
  {
    return;
  }
  else if (depth == 1)
  // Reached destination
  {
    if (isleafs)
    {
      for (int i = 0; i < NODE_SIZE; ++i)
      {
        if (node->leafs[i])
          eth_fprintf(out, "~w ", node->leafs[i]);
        else
          fprintf(out, "- ");
      }
    }
    else
    {
      for (int i = 0; i < NODE_SIZE; ++i)
      {
        if (node->leafs[i])
          fprintf(out, "+ ");
        else
          fprintf(out, "- ");
      }
    }
  }
  else
  // Go deeper
  {
    for (int i = 0; i < NODE_SIZE; ++i)
      print_nodes_at(node->subnodes[i], depth - 1, out, isleafs);
  }
}

static void
print_tree(const vector_tree *tree, FILE *out)
{
  fprintf(out, "tree depth: %d\n", tree->depth);
  for (int i = 1; i <= tree->depth; ++i)
  {
    fprintf(out, "level %2d:\t", i);
    print_nodes_at(tree->root, i, out, i == tree->depth);
    fprintf(out, "\n");
  }
}

/*static void
test_tree()
{
  eth_use_symbol(A)
  eth_use_symbol(B)
  eth_use_symbol(C)
  eth_use_symbol(D)
  eth_use_symbol(E)
  eth_use_symbol(F)
  eth_use_symbol(G)

  vector_tree *tree = create_tree();
  vector_node *node;

  eth_debug("--- A ---");
  node = create_node();
  for (int i = 0; i < NODE_SIZE; ++i)
    eth_ref(node->leafs[i] = A);
  push_node(tree, node);
  print_tree(tree, stdout);

  eth_debug("--- B ---");
  node = create_node();
  for (int i = 0; i < NODE_SIZE; ++i)
    eth_ref(node->leafs[i] = B);
  push_node(tree, node);
  print_tree(tree, stdout);

  eth_debug("--- C ---");
  node = create_node();
  for (int i = 0; i < NODE_SIZE; ++i)
    eth_ref(node->leafs[i] = C);
  push_node(tree, node);
  print_tree(tree, stdout);

  eth_debug("--- D ---");
  node = create_node();
  for (int i = 0; i < NODE_SIZE; ++i)
    eth_ref(node->leafs[i] = D);
  push_node(tree, node);
  print_tree(tree, stdout);

  eth_debug("--- E ---");
  node = create_node();
  for (int i = 0; i < NODE_SIZE; ++i)
    eth_ref(node->leafs[i] = E);
  push_node(tree, node);
  print_tree(tree, stdout);

  eth_debug("--- F ---");
  node = create_node();
  for (int i = 0; i < NODE_SIZE; ++i)
    eth_ref(node->leafs[i] = F);
  push_node(tree, node);
  print_tree(tree, stdout);

  eth_debug("--- G ---");
  node = create_node();
  for (int i = 0; i < NODE_SIZE; ++i)
    eth_ref(node->leafs[i] = G);
  push_node(tree, node);
  print_tree(tree, stdout);

  destroy_tree(tree);
} */
