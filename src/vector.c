/* Copyright (C) 2020  Ivan Pidhurskyi
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "ether/ether.h"

#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <string.h>


ETH_MODULE("ether:vector")


#define NODE_SIZE_LOG2 ETH_VECTOR_SLICE_SIZE_LOG2
#define NODE_SIZE ETH_VECTOR_SLICE_SIZE


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
    { // we are on a leaf-node, so just unref all values
      for (int i = 0; i < NODE_SIZE; ++i)
      {
        if (node->leafs[i])
          eth_unref(node->leafs[i]);
        else
          break;
      }
    }
    else
    { // we are on an intermediate node, so recursively unref all branches
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
  return curr;
}

static vector_node*
copy_path_aux(const vector_node *curr, int log2cap, int lvl, int *restrict k,
    vector_node **leaf)
{
  if (lvl > 1)
  {
    log2cap -= NODE_SIZE_LOG2;
    int isub = *k >> log2cap;
    *k -= isub << log2cap;

    vector_node *newnode = copy_branch(curr);
    vector_node *newsubnode =
      copy_path_aux(curr->subnodes[isub], log2cap, lvl-1, k, leaf);

    dec_node(newnode->subnodes[isub]);
    ref_node(newnode->subnodes[isub] = newsubnode);
    return newnode;
  }
  return *leaf = copy_leaf(curr);
}

static vector_node*
copy_path(vector_tree *tree, int *restrict k)
{
  /* No copying untill encountered a node with multiple references. */
  vector_node *curr = tree->root;
  vector_node *prev = NULL;
  int log2cap = NODE_SIZE_LOG2 * tree->depth;
  int lvl, isub = -1;
  for (lvl = tree->depth; lvl > 1 and curr->rc == 1; --lvl)
  {
    log2cap -= NODE_SIZE_LOG2;
    isub = *k >> log2cap;
    *k -= isub << log2cap;
    prev = curr;
    curr = curr->subnodes[isub];
  }

  if (lvl == 1)
  {
    if (curr->rc == 1)
    {
      /* Nothing has to be copied. Just update the leaf. */
      return curr;
    }
    else
    {
      /* Copy leaf-node. */
      vector_node *newleaf = copy_leaf(curr);
      ref_node(newleaf);
      if (prev)
      {
        dec_node(prev->subnodes[isub] /* = curr */);
        prev->subnodes[isub] = newleaf;
      }
      else /* root-node is a leaf-node */
      {
        dec_node(tree->root);
        tree->root = newleaf;
      }
      return newleaf;
    }
  }
  else /* lvl > 1 => curr->rc != 1 */
  {
    /* Copy the path to the desired leaf. */
    vector_node *newleaf;
    vector_node *newpath = copy_path_aux(curr, log2cap, lvl, k, &newleaf);
    ref_node(newpath);
    if (prev == NULL /* we copied the root */)
    {
      assert(tree->root->rc > 1);
      dec_node(tree->root);
      tree->root = newpath;
    }
    else /* copied subpath */
    {
      assert(prev->subnodes[isub]->rc > 1);
      dec_node(prev->subnodes[isub]);
      prev->subnodes[isub] = curr;
    }
    return newleaf;
  }
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
    ref_node(tree->root = node);
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

static void
insert(vector *vec, int k, eth_t x)
{
  if (k >= vec->tree.size)
  {
    /* Insert in the tail. */
    k -= vec->tree.size;
    if (vec->tail->rc > 1)
    {
      /* Have to copy the tail-node. */
      vector_node *newtail = copy_leaf(vec->tail);
      ref_node(newtail);
      dec_node(vec->tail);
      vec->tail = newtail;
    }
    eth_ref(x);
    eth_unref(vec->tail->leafs[k]);
    vec->tail->leafs[k] = x;
  }
  else /* k < vec->tree.size */
  {
    /* Insert in the tree. */
    vector_node *leaf = copy_path(&vec->tree, &k);
    eth_ref(x);
    eth_unref(leaf->leafs[k]);
    leaf->leafs[k] = x;
  }
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

void __attribute__((flatten))
eth_insert_mut(eth_t v, int k, eth_t x)
{
  insert(VECTOR(v), k, x);
}

eth_t __attribute__((flatten))
eth_insert_pers(eth_t v, int k, eth_t x)
{
  vector *vec = copy_vector(VECTOR(v));
  insert(vec, k, x);
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

void
eth_vector_begin(eth_t v, eth_vector_iterator *iter, int start)
{
  vector *vec = VECTOR(v);

  iter->vec = v;
  if (start < eth_vec_len(v))
  {
    iter->isend = false;

    vector* v = VECTOR(vec);
    if (start >= v->tree.size)
    { // tail slice
      iter->slice.begin = v->tail->leafs + start - v->tree.size;
      iter->slice.end = iter->slice.begin + v->tailsize;
      iter->slice.offset = v->tree.size;
    }
    else
    { // inside the tree
      int k = start;
      vector_node *node = find_node(&v->tree, &k);
      iter->slice.begin = node->leafs + k;
      iter->slice.end = node->leafs + NODE_SIZE; // all slices inside the tree
                                                 // body are full
      iter->slice.offset = start - k;
    }
  }
  else
  {
    iter->isend = true;
  }
}

bool
eth_vector_next(eth_vector_iterator *iter)
{

  if (eth_unlikely(iter->isend))
    return false;

  vector *v = VECTOR(iter->vec);
  int slicelen = iter->slice.end - iter->slice.begin;
  int nextoffs = iter->slice.offset + slicelen;
  if (nextoffs >= v->tree.size + v->tailsize)
  { // no more slices
    iter->isend = true;
    return false;
  }
  else if (nextoffs >= v->tree.size)
  { // tail slice
    iter->slice.begin = v->tail->leafs;
    iter->slice.end = iter->slice.begin + v->tailsize;
    iter->slice.offset = v->tree.size;
    return true;
  }
  else
  { // inside the tree
    int k = nextoffs;
    vector_node *node = find_node(&v->tree, &k);
    assert(k == 0);
    iter->slice.begin = node->leafs;
    iter->slice.end = node->leafs + NODE_SIZE; // all slices inside the tree
                                               // body are full
    iter->slice.offset = nextoffs;
    return true;
  }
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

