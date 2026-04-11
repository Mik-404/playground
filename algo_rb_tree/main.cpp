#include <stdint.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

const int64_t cInf = 1000000010;
const int64_t cInpLen = 10;

enum Color { BLACK, RED, EMPTYBLACK, ROOT };

class Node {
  Color color_;
  Node* left_child_;
  Node* right_child_;
  Node* parent_;

  int64_t depth_;
  int64_t value_;

  void RecalcDepth() {
    this->depth_ = this->left_child_->depth_ + this->right_child_->depth_ + 1;
  }

  void SwapNodes() {
    Node* ptr_grandpa = this->parent_->parent_;
    Node* ptr_fa = this->parent_;
    this->parent_->parent_ = this;
    if (this == this->parent_->left_child_) {
      this->parent_->left_child_ = this->right_child_;
      this->right_child_->parent_ = this->parent_;
      this->right_child_ = this->parent_;
    } else {
      this->parent_->right_child_ = this->left_child_;
      this->left_child_->parent_ = this->parent_;
      this->left_child_ = this->parent_;
    }
    this->parent_ = ptr_grandpa;
    if (ptr_grandpa->left_child_ == ptr_fa) {
      ptr_grandpa->left_child_ = this;
    } else {
      ptr_grandpa->right_child_ = this;
    }
    ptr_fa->RecalcDepth();
    this->RecalcDepth();
  }

  void InsertRePaintL() {
    this->parent_->color_ = BLACK;
    this->parent_->parent_->right_child_->color_ = BLACK;
    this->parent_->parent_->color_ = RED;
    this->parent_->parent_->NormalizeInsert();
  }

  void InsertRePaintR() {
    this->parent_->color_ = BLACK;
    this->parent_->parent_->left_child_->color_ = BLACK;
    this->parent_->parent_->color_ = RED;
    this->parent_->parent_->NormalizeInsert();
  }

  void InsertRepaintFlick() {
    this->parent_->color_ = BLACK;
    this->parent_->parent_->color_ = RED;
    this->parent_->SwapNodes();
  }

  void NormalizeInsert() {
    if (this->parent_->color_ == ROOT) {
      this->color_ = BLACK;
      return;
    }
    if (this->color_ != this->parent_->color_) {
      return;
    }
    if (this->parent_->parent_->left_child_ == this->parent_) {
      if (this->parent_->parent_->right_child_->color_ == RED) {
        this->InsertRePaintL();
      } else {
        if (this->parent_->left_child_ == this) {
          this->InsertRepaintFlick();
        } else {
          this->SwapNodes();
          this->left_child_->InsertRepaintFlick();
        }
      }
    } else {
      if (this->parent_->parent_->left_child_->color_ == RED) {
        this->InsertRePaintR();
      } else {
        if (this->parent_->right_child_ == this) {
          this->InsertRepaintFlick();
        } else {
          this->SwapNodes();
          this->right_child_->InsertRepaintFlick();
        }
      }
    }
  }

  Node* Find(int64_t x, bool get_place = false, int64_t depth_add = 0) {
    if (this->color_ == ROOT) {
      if (this->left_child_ == nullptr) {
        if (get_place) {
          return this;
        }
        return nullptr;
      }
      return this->left_child_->Find(x, get_place, depth_add);
    }
    if (this->value_ == x) {
      return this;
    }
    this->depth_ += depth_add;
    if (this->value_ > x) {
      if (this->left_child_->color_ != EMPTYBLACK) {
        return this->left_child_->Find(x, get_place, depth_add);
      }
      if (get_place) {
        return this;
      }
      return nullptr;
    }
    if (this->right_child_->color_ != EMPTYBLACK) {
      return this->right_child_->Find(x, get_place, depth_add);
    }
    if (get_place) {
      return this;
    }
    return nullptr;
  }

  void LeftNormalization() {
    if (this->parent_->color_ == RED) {
      if (this->parent_->right_child_->left_child_->color_ == RED) {
        this->parent_->color_ = BLACK;
        this->parent_->right_child_->left_child_->SwapNodes();
        this->parent_->right_child_->SwapNodes();
      } else if (this->parent_->right_child_->right_child_->color_ == RED) {
        this->parent_->right_child_->SwapNodes();
      } else {
        this->parent_->color_ = BLACK;
        this->parent_->right_child_->color_ = RED;
      }
    } else {
      if (this->parent_->right_child_->color_ == RED) {
        if (this->parent_->right_child_->left_child_->right_child_->color_ ==
            RED) {
          this->parent_->right_child_->left_child_->right_child_->color_ =
              BLACK;
          ;
          this->parent_->right_child_->left_child_->SwapNodes();
          this->parent_->right_child_->SwapNodes();
        } else if (this->parent_->right_child_->left_child_->left_child_
                       ->color_ == RED) {
          this->parent_->right_child_->left_child_->left_child_->color_ = BLACK;
          this->parent_->right_child_->left_child_->left_child_->SwapNodes();
          this->parent_->right_child_->left_child_->SwapNodes();
          this->parent_->right_child_->SwapNodes();
        } else {
          this->parent_->right_child_->color_ = BLACK;
          this->parent_->right_child_->left_child_->color_ = RED;
          this->parent_->right_child_->SwapNodes();
        }
      } else {
        if (this->parent_->right_child_->left_child_->color_ == RED) {
          this->parent_->right_child_->left_child_->color_ = BLACK;
          this->parent_->right_child_->left_child_->SwapNodes();
          this->parent_->right_child_->SwapNodes();
        } else if (this->parent_->right_child_->right_child_->color_ == RED) {
          this->parent_->right_child_->right_child_->color_ = BLACK;
          this->parent_->right_child_->SwapNodes();
        } else {
          this->parent_->right_child_->color_ = RED;
          this->parent_->NormalizeBlackErasing();
        }
      }
    }
  }

  void RightNormalization() {
    if (this->parent_->color_ == RED) {
      if (this->parent_->left_child_->right_child_->color_ == RED) {
        this->parent_->color_ = BLACK;
        this->parent_->left_child_->right_child_->SwapNodes();
        this->parent_->left_child_->SwapNodes();
      } else if (this->parent_->left_child_->left_child_->color_ == RED) {
        this->parent_->left_child_->SwapNodes();
      } else {
        this->parent_->color_ = BLACK;
        this->parent_->left_child_->color_ = RED;
      }
    } else {
      if (this->parent_->left_child_->color_ == RED) {
        if (this->parent_->left_child_->right_child_->left_child_->color_ ==
            RED) {
          this->parent_->left_child_->right_child_->left_child_->color_ = BLACK;
          ;
          this->parent_->left_child_->right_child_->SwapNodes();
          this->parent_->left_child_->SwapNodes();
        } else if (this->parent_->left_child_->right_child_->right_child_
                       ->color_ == RED) {
          this->parent_->left_child_->right_child_->right_child_->color_ =
              BLACK;
          this->parent_->left_child_->right_child_->right_child_->SwapNodes();
          this->parent_->left_child_->right_child_->SwapNodes();
          this->parent_->left_child_->SwapNodes();
        } else {
          this->parent_->left_child_->color_ = BLACK;
          this->parent_->left_child_->right_child_->color_ = RED;
          this->parent_->left_child_->SwapNodes();
        }
      } else {
        if (this->parent_->left_child_->right_child_->color_ == RED) {
          this->parent_->left_child_->right_child_->color_ = BLACK;
          this->parent_->left_child_->right_child_->SwapNodes();
          this->parent_->left_child_->SwapNodes();
        } else if (this->parent_->left_child_->left_child_->color_ == RED) {
          this->parent_->left_child_->left_child_->color_ = BLACK;
          this->parent_->left_child_->SwapNodes();
        } else {
          this->parent_->left_child_->color_ = RED;
          this->parent_->NormalizeBlackErasing();
        }
      }
    }
  }

  void NormalizeBlackErasing() {
    if (this->parent_->color_ == ROOT) {
      return;
    }
    if (this->parent_->left_child_ == this) {
      this->LeftNormalization();
    } else {
      this->RightNormalization();
    }
  }

  void DeleteBlackBlackCase() {
    if (this->parent_->color_ == ROOT) {
      this->parent_->left_child_ = nullptr;
      delete this;
      return;
    }
    if (this == this->parent_->left_child_) {
      this->parent_->left_child_ = new Node(0, EMPTYBLACK, this->parent_);
      if (this->color_ == BLACK) {
        this->parent_->left_child_->NormalizeBlackErasing();
      }
    } else {
      this->parent_->right_child_ = new Node(0, EMPTYBLACK, this->parent_);
      if (this->color_ == BLACK) {
        this->parent_->right_child_->NormalizeBlackErasing();
      }
    }
    delete this;
  }

  void DeleteLeaf() {
    if (this->left_child_->color_ == EMPTYBLACK &&
        this->right_child_->color_ == EMPTYBLACK) {
      this->DeleteBlackBlackCase();
    } else if (this->left_child_->color_ == EMPTYBLACK) {
      this->value_ = this->right_child_->value_;
      delete this->right_child_;
      this->right_child_ = new Node(0, EMPTYBLACK, this);
    } else {
      this->value_ = this->left_child_->value_;
      delete this->left_child_;
      this->left_child_ = new Node(0, EMPTYBLACK, this);
    }
  }

  Node* FindReplacement() {
    Node* cur_child = nullptr;
    if (this->left_child_->depth_ > this->right_child_->depth_) {
      cur_child = this->left_child_;
      while (cur_child->right_child_->color_ != EMPTYBLACK) {
        cur_child->depth_--;
        cur_child = cur_child->right_child_;
      }
    } else {
      cur_child = this->right_child_;
      while (cur_child->left_child_->color_ != EMPTYBLACK) {
        cur_child->depth_--;
        cur_child = cur_child->left_child_;
      }
    }
    cur_child->depth_--;
    return cur_child;
  }

  int64_t Drop(bool left) {
    Node* cur_child = this->right_child_;
    if (left) {
      cur_child = this->left_child_;
    }
    while (cur_child->color_ != EMPTYBLACK &&
           (left ? cur_child->right_child_->color_
                 : cur_child->left_child_->color_) != EMPTYBLACK) {
      if (left) {
        cur_child = cur_child->right_child_;
      } else {
        cur_child = cur_child->left_child_;
      }
    }
    if (cur_child->color_ != EMPTYBLACK) {
      return cur_child->value_;
    }
    return cInf;
  }

 public:
  void Insert(int64_t x) {
    Node* node = this->Find(x, true);
    if (node->color_ != ROOT && node->value_ == x) {
      return;
    }
    this->Find(x, true, 1);
    if (node->color_ == ROOT) {
      node->left_child_ = new Node(x, BLACK, node);
      return;
    }
    if (node->value_ > x) {
      delete node->left_child_;
      node->left_child_ = new Node(x, RED, node);
      node->left_child_->NormalizeInsert();
    } else {
      delete node->right_child_;
      node->right_child_ = new Node(x, RED, node);
      node->right_child_->NormalizeInsert();
    }
  }

  void Erase(int64_t x) {
    Node* node = this->Find(x);
    if (node == nullptr) {
      return;
    }
    this->Find(x, false, -1);
    node->depth_--;
    if (node->left_child_->color_ != EMPTYBLACK &&
        node->right_child_->color_ != EMPTYBLACK) {
      Node* cur_child = node->FindReplacement();
      node->value_ = cur_child->value_;
      node = cur_child;
    }
    node->DeleteLeaf();
  }

  bool Exists(int64_t x) {
    Node* node = this->Find(x);
    return node != nullptr;
  }

  int64_t FindUpperBound(int64_t x) {
    if (this->color_ == ROOT) {
      return this->left_child_ == nullptr
                 ? cInf
                 : this->left_child_->FindUpperBound(x);
    }
    if (this->value_ == x) {
      return this->Drop(false);
    }
    if (this->value_ < x) {
      if (this->right_child_->color_ != EMPTYBLACK) {
        return this->right_child_->FindUpperBound(x);
      }
      return cInf;
    }
    int64_t ans = cInf;
    if (this->left_child_->color_ != EMPTYBLACK) {
      ans = this->left_child_->FindUpperBound(x);
    }
    if (ans == cInf) {
      return this->value_;
    }
    return ans;
  }

  int64_t FindLowerBound(int64_t x) {
    if (this->color_ == ROOT) {
      return this->left_child_ == nullptr
                 ? cInf
                 : this->left_child_->FindLowerBound(x);
    }
    if (this->value_ == x) {
      return this->Drop(true);
    }
    if (this->value_ > x) {
      if (this->left_child_->color_ != EMPTYBLACK) {
        return this->left_child_->FindLowerBound(x);
      }
      return cInf;
    }
    int64_t ans = cInf;
    if (this->right_child_->color_ != EMPTYBLACK) {
      ans = this->right_child_->FindLowerBound(x);
    }
    if (ans == cInf) {
      return this->value_;
    }
    return ans;
  }

  int64_t KthOrderStat(int64_t k) {
    if (this->color_ == ROOT) {
      return this->left_child_ == nullptr ? cInf
                                          : this->left_child_->KthOrderStat(k);
    }
    if (this->left_child_ == nullptr) {
      assert(false);
      return cInf;
    }
    if (k <= this->left_child_->depth_) {
      return this->left_child_->KthOrderStat(k);
    }
    if (k == this->left_child_->depth_ + 1) {
      return this->value_;
    }
    if (k <= this->left_child_->depth_ + this->right_child_->depth_ + 1) {
      return this->right_child_->KthOrderStat(k - this->left_child_->depth_ -
                                              1);
    }
    return cInf;
  }

  Node(int64_t value, Color color, Node* parent)
      : color_(color),
        left_child_(nullptr),
        right_child_(nullptr),
        parent_(parent),
        depth_(0),
        value_(value) {
    if (this->color_ != EMPTYBLACK && this->color_ != ROOT) {
      depth_ = 1;
      left_child_ = new Node(0, EMPTYBLACK, this);
      right_child_ = new Node(0, EMPTYBLACK, this);
    }
  }

  ~Node() {
    delete left_child_;
    delete right_child_;
  }
};

int main() {
  std::ios_base::sync_with_stdio(false);
  std::cin.tie(0);
  std::cout.tie(0);
  Node* root = new Node(0, ROOT, nullptr);
  char input[cInpLen];
  while (std::cin >> input) {
    if (input[0] == 'i') {
      int64_t x;
      std::cin >> x;
      root->Insert(x);
    } else if (input[0] == 'd') {
      int64_t x;
      std::cin >> x;
      root->Erase(x);
    } else if (input[0] == 'e') {
      int64_t x;
      std::cin >> x;
      std::cout << std::boolalpha;
      std::cout << root->Exists(x) << '\n';
    } else if (input[0] == 'n') {
      int64_t x;
      std::cin >> x;
      int64_t res = root->FindUpperBound(x);
      if (res == cInf) {
        std::cout << "none" << '\n';
      } else {
        std::cout << res << '\n';
      }
    } else if (input[0] == 'p') {
      int64_t x;
      std::cin >> x;
      int64_t res = root->FindLowerBound(x);
      if (res == cInf) {
        std::cout << "none" << '\n';
      } else {
        std::cout << res << '\n';
      }
    } else {
      int64_t x;
      std::cin >> x;
      int64_t res = root->KthOrderStat(x + 1);
      if (res == cInf) {
        std::cout << "none" << '\n';
      } else {
        std::cout << res << '\n';
      }
    }
  }
  delete root;
}