/*
   eXokernel Development Kit (XDK)

   Based on code by Samsung Research America Copyright (C) 2013
 
   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.

   As a special exception, if you link the code in this file with
   files compiled with a GNU compiler to produce an executable,
   that does not cause the resulting executable to be covered by
   the GNU Lesser General Public License.  This exception does not
   however invalidate any other reasons why the executable file
   might be covered by the GNU Lesser General Public License.
   This exception applies to code released by its copyright holders
   in files containing the exception.  
*/

/*
  Authors:
  Copyright (C) 2012, Daniel G. Waddington <daniel.waddington@acm.org>
  Copyright (C) 2011, Chen Tian
*/

#ifndef __EXO_AVL_TREE_H__
#define __EXO_AVL_TREE_H__

#include <printf.h>

namespace Exokernel
{
  /** Comparison types. */
  enum cmp_t {
    MIN_CMP = -1,   //>! less than
    EQ_CMP  = 0,    //>! equal to
    MAX_CMP = 1     //>! greater than
  };

  enum balance_t { LEFT_HEAVY = -1, BALANCED = 0, RIGHT_HEAVY = 1 };

  enum height_effect_t { HEIGHT_NOCHANGE = 0, HEIGHT_CHANGE = 1 };

  enum dir_t { LEFT = 0, RIGHT = 1};

  /** Type of search. */
  enum search_t {
    REG_OVERLAP, 
    REG_CONTAINMENT, 
    REG_EMPTY, 
    REG_MATCH,
  };

  enum traversal_order_t { LTREE, KEY, RTREE };

  /** 
   * A node in an AVL tree. 
   */
  template <class T>
  class AVL_node {

    /* utility functions */
    inline static int
    MIN(int a, int b) {
      return  (a < b) ? a : b;
    }

    inline static int
    MAX(int a, int b) {
      return  (a > b) ? a : b;
    }

    /** Returns true if the tree is too heavy on the left side. */
    inline static int
    LEFT_IMBALANCE(short bal) { return (bal < LEFT_HEAVY); }

    /** Returns true if the tree is too heavy on the right side. */
    inline static int
    RIGHT_IMBALANCE(short bal) { return (bal > RIGHT_HEAVY); }


  public:
    AVL_node<T> *subtree[2];

    /**
     * The balance factor. 
     * - if -1, left subtree is taller than right subtree;
     * - if  0, the height of two subtrees are equal;
     * - if  1, right subtree is taller than left subtree.
     */
    short bf;

  private:
    
    /** 
     * Comparison operator which must be provisioned by the
     * superclass for the node
     * 
     * @param n 
     * 
     * @return 
     */
    virtual bool higher(AVL_node<T> *n) = 0;
    //      return (static_cast<T*>(this)->higher(static_cast<T*>(n)));
    //    }
  
    /** 
     * The actual compare function
     *
     * If n.key < this.key return MIN_CMP
     * else if n.key > this.key return MAX_CMP
     * else return EQ_CMP;
     */
    cmp_t compare(AVL_node<T> *n) {
      return (higher(n) ? MIN_CMP : 
              n->higher(this) ? MAX_CMP : EQ_CMP );
    }

    /** 
     *  The compare function.
     *  It merges the data comparision and successor search.
     *  I really don't like it. :(
     */
    cmp_t compare(AVL_node *n, cmp_t cmp) {
      switch (cmp) {
      case EQ_CMP :
        return  (static_cast<T*>(this)->compare(static_cast<T*>(n)));
      case MIN_CMP :  // Find the minimal element in this tree
        return  (subtree[LEFT] == NULL) ? EQ_CMP : MIN_CMP;
      case MAX_CMP :  // Find the maximal element in this tree
        return  (subtree[RIGHT] == NULL) ? EQ_CMP : MAX_CMP;
      }
      panic("unexpected error!");
      return EQ_CMP;
    }

    /** Gets opposite direction.  */
    static dir_t opposite(dir_t dir) { 
      return dir_t(1 - int(dir));
    }

    /** Performs a single rotation (LL case or RR case). */
    static int rotate_once(AVL_node * & root, dir_t dir) {
      dir_t  otherDir = opposite(dir);
      AVL_node * oldRoot = root;

      int  heightChange = (root->subtree[otherDir]->bf == 0)
        ? HEIGHT_NOCHANGE
        : HEIGHT_CHANGE;

      root = oldRoot->subtree[otherDir];

      oldRoot->subtree[otherDir] = root->subtree[dir];
      root->subtree[dir] = oldRoot;

      oldRoot->bf = -((dir == LEFT) ? --(root->bf) : ++(root->bf));

      return  heightChange;
    }


    /** Performs double rotation (RL case or LR case). */
    static int rotate_twice(AVL_node * & root, dir_t dir) {
      dir_t  otherDir = opposite(dir);
      AVL_node * oldRoot = root;
      AVL_node * oldOtherDirSubtree = root->subtree[otherDir];

      // assign new root
      root = oldRoot->subtree[otherDir]->subtree[dir];

      oldRoot->subtree[otherDir] = root->subtree[dir];
      root->subtree[dir] = oldRoot;

      oldOtherDirSubtree->subtree[dir] = root->subtree[otherDir];
      root->subtree[otherDir] = oldOtherDirSubtree;

      root->subtree[LEFT]->bf = -MAX(root->bf, 0);
      root->subtree[RIGHT]->bf = -MIN(root->bf, 0);
      root->bf = 0;

      return  HEIGHT_CHANGE;
    }


    /** 
     * Rebalances a tree.  
     * @param root the root of the tree.
     */
    static int rebalance (AVL_node * & root) {
      int  heightChange = HEIGHT_NOCHANGE;

      if (LEFT_IMBALANCE(root->bf)) {
        if (root->subtree[LEFT]->bf==  RIGHT_HEAVY) {
          //panic("LR case");
          heightChange = rotate_twice(root, RIGHT);
        } 
        else {
          //panic("LL case");
          heightChange = rotate_once(root, RIGHT);
        }
      } 
      else if (RIGHT_IMBALANCE(root->bf)) {
        if (root->subtree[RIGHT]->bf==  LEFT_HEAVY) {
          //panic("RL case");
          heightChange = rotate_twice(root, LEFT);
        } 
        else {
          //panic("RR case");
          heightChange = rotate_once(root, LEFT);
        }
      }
      return  heightChange;
    }

    /** 
     * Inserts a node into a tree. 
     * @param n the node.
     * @param root the root of the tree.  
     * @param change
     * @returns 
     */
    static AVL_node* insert(AVL_node *n, AVL_node*& root, int& change) {

      if (root == NULL) {
        root = n;
        change = HEIGHT_CHANGE;
        return NULL;
      }

      int increase = 0;
      AVL_node *ret;

      cmp_t result = root->compare(n, EQ_CMP);
      dir_t  dir = (result == MIN_CMP) ? LEFT : RIGHT;

      if (result != EQ_CMP) {
        ret = insert(n, root->subtree[dir], change);
        if (ret) {
          panic("inserting the same node!");
        }
        increase = result * change;
      }
      else {
        increase = HEIGHT_NOCHANGE;
        panic("inserting a conflicting region!");
        return root;
      }

      root->bf += increase;
      change = (increase && root->bf) ? 
        (1 - rebalance(root)) : HEIGHT_NOCHANGE;
      return NULL;
    }


    /** 
     * Removes a node from a tree. 
     * @param n the node. 
     * @param root the root of the tree. 
     * @param change
     * @param cmp
     * @returns 
     */
    static AVL_node* remove(AVL_node *n, AVL_node *& root, int & change, 
                            cmp_t cmp) {
      if (root == NULL) {
        change = HEIGHT_NOCHANGE;
        return  NULL;
      }

      int decrease = 0;
      AVL_node *ret;

      cmp_t result = root->compare(n, cmp);
      dir_t  dir = (result == MIN_CMP) ? LEFT : RIGHT;

      if (result != EQ_CMP) {
        ret = remove(n, root->subtree[dir], change, cmp);
        if (!ret) {
          panic("Node cannot be found!");
        }
        decrease = result * change;
      }
      else {
        ret = root;
        if ((root->subtree[LEFT] == NULL) &&
            (root->subtree[RIGHT] == NULL)) {
          root = NULL; 
          change = HEIGHT_CHANGE;    
          return  ret;
        }
        else if ((root->subtree[LEFT] == NULL) ||
                 (root->subtree[RIGHT] == NULL)) {
          root = root->subtree[(root->subtree[RIGHT]) ? RIGHT : LEFT];
          change = HEIGHT_CHANGE;
          return  ret;
        } 
        else {
          root = remove(n, root->subtree[RIGHT], decrease, MIN_CMP);
          root->subtree[LEFT] = ret->subtree[LEFT];
          root->subtree[RIGHT] = ret->subtree[RIGHT];
          root->bf = ret->bf;
          //printf("successor is at %p\n", root);
        } 
      }
      root->bf -= decrease;   
      if (decrease) {
        if (root->bf) {
          change = rebalance(root);
        } 
        else {
          change = HEIGHT_CHANGE; 
        }
      } 
      else {
        change = HEIGHT_NOCHANGE;
      }
      return ret;
    }


    /** Returns the height of this node in its tree. */
    int height() const {
      int  l = (subtree[LEFT])  ? subtree[LEFT]->height()  : 0;
      int  r = (subtree[RIGHT])  ? subtree[RIGHT]->height()  : 0;
      return  (1 + MAX(l, r));
    }

    /** Validates the position of this node. */
    bool validate() {
      bool valid = true;

      if (subtree[LEFT])   valid = valid && subtree[LEFT]->validate();
      if (subtree[RIGHT])  valid = valid && subtree[RIGHT]->validate();

      int  l = (subtree[LEFT])  ? subtree[LEFT]->height()  : 0;
      int  r = (subtree[RIGHT]) ? subtree[RIGHT]->height()  : 0;

      int diff = r - l;

      if (LEFT_IMBALANCE(diff) || RIGHT_IMBALANCE(diff))
        valid = false;

      if (diff != bf) 
        valid = false;

      if (subtree[LEFT] 
          && subtree[LEFT]->compare(this) == MIN_CMP)
        valid = false;

      if (subtree[RIGHT] 
          && subtree[RIGHT]->compare(this) == MAX_CMP)
        valid = false;

      return valid;
    }


    ///////////////////////////////////////////////////////////////////////////
    // Public interface starts here!
    ///////////////////////////////////////////////////////////////////////////

  public:

    /** Constructor. */
    AVL_node(): bf(0) {
      subtree[LEFT] = subtree[RIGHT] = NULL;
    } 

    /** 
     * Inserts a node into a tree. 
     * @param n the node.
     * @param root the root of the tree. 
     */
    static void insert(AVL_node *n, AVL_node* & root) {
      int change;
      insert(n, root, change);
      return;
    }

    /** 
     * Removes a node from a tree. 
     * @param n the node.
     * @param root the root of the tree. 
     */
    static void remove(AVL_node *n, AVL_node* & root) {
      int change;
      remove(n, root, change, EQ_CMP);
      return;
    }

    /** 
     * Validates a tree. 
     * @param root the root of a tree. 
     */
    static bool validate(AVL_node *const root) {
      if (!root)
        return true;
      else
        return root->validate();
    }

  };


  /** 
   * An AVL tree. 
   */
  template <class T>
  class AVL_tree {

  protected:
    /** Root of the tree.*/
    AVL_node <T>* _root;

  public:

    /** Constructor. */
    AVL_tree(): _root(NULL) {}

    /** Destructor. */
    ~AVL_tree() { 
      //      panic("todo"); 
      _root = NULL;
    } 

  private:

    static void indent (int len) {
      for (int i = 0; i < len; i++) { 
        printf("  ");
      }
    }
  

    static void dump(traversal_order_t order, AVL_node<T> * node, int level=0) {
      int verbose = 0;
      unsigned  len = (level * 5) + 1;

      if ((order == LTREE) && (node->subtree[LEFT] == NULL)) {
        indent(len);
        printf("         **NULL**\n");
      }
      if (order == KEY) {
        indent(len);
        if (verbose) {
          static_cast<T*>(node)->dump();
          printf("@[%p] bf=%x\n", node, (unsigned)node->bf); 
        }
        else {
          static_cast<T*>(node)->dump();
          printf("\n"); 
        }
      }
      if ((order == RTREE) && (node->subtree[RIGHT] == NULL)) {
        indent(len);
        printf("         **NULL**\n");
      }

    }

  public:
    static void dump (AVL_node<T> * node, int level = 0) {
      if (node == NULL) {
        printf("***EMPTY TREE***\n");
      } 
      else {
        dump(RTREE, node, level);
        if (node->subtree[RIGHT]  !=  NULL) {
          dump(node->subtree[RIGHT], level+1);
        }
        dump(KEY, node, level);
        if (node->subtree[LEFT]  !=  NULL) {
          dump(node->subtree[LEFT], level+1);
        }
        dump(LTREE, node, level);
      }
    }

    typedef void (*visitor_function_t)(AVL_node<T>& node);

  public:
    void traverse(visitor_function_t f) {
      panic("TODO");
    }


  public:

    /** Returns true if the tree is empty; false otherwise. */
    bool is_empty() {
      return _root==NULL;
    }   
  
    /** 
     * Inserts a node into the tree.
     * @param n the node. 
     */
    void insert_node(AVL_node <T> *n) {
      AVL_node<T>::insert(n, _root);
    }

    /** 
     * Removes a node from the tree.
     * @param n the node. 
     */
    void remove_node(AVL_node <T> *n) {
      AVL_node<T>::remove(n, _root);
    }


    /** Checks whether or not the tree is valid. */
    bool validate() {
      if (!AVL_node<T>::validate(_root)) {
        return false;
      }
      else {
        return true;
      }
    }

    size_t total_size() {
      return static_cast<T*>(_root)->total_size();
    }

    void dump() {
      assert(_root);
      printf("======== dumping AVL tree\n");
      dump(_root);
    }

    AVL_node <T>* get_root() const { return _root; }  
  };

}

#endif // __EXO_AVL_TREE_H__
