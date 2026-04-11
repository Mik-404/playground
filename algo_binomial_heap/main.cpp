#include <iostream>
#include <vector>

using std::cout, std::cin, std::vector, std::pair;
const int cInfty = 1000000002;
const int cArraySize = 1005000;
const int cHeapsCount = 2000;

class BinomialHeap {
  struct Node {
    Node* parent = nullptr;
    Node* right_neighbour = nullptr;
    Node* left_child = nullptr;
    int value = cInfty;
    int degree = 0;
    bool empty = true;
    int great_num = 0;
  };

  static void ReHung(Node*& first_node, Node* second_node) {
    if (first_node->value < second_node->value ||
        (first_node->value == second_node->value &&
         first_node->great_num < second_node->great_num)) {
      first_node->degree += 1;
      second_node->parent = first_node;
      second_node->right_neighbour = first_node->left_child;
      first_node->left_child = second_node;
    } else {
      second_node->degree += 1;
      second_node->parent = first_node->parent;

      second_node->right_neighbour = first_node->right_neighbour;

      first_node->right_neighbour = second_node->left_child;
      first_node->parent = second_node;
      second_node->left_child = first_node;
      first_node = second_node;
    }
  }

  static void ReverseChilds(Node* node) {
    Node* cur_child = node->left_child;
    Node* before_element = nullptr;
    Node* blank = nullptr;
    while (cur_child != nullptr) {
      blank = cur_child->right_neighbour;
      cur_child->right_neighbour = before_element;
      before_element = cur_child;
      cur_child = blank;
    }
    node->left_child = before_element;
  }

  static void Merge(Node* base_heap, Node* merging_node) {
    if (merging_node == nullptr || base_heap == nullptr) {
      return;
    }
    Node* before_element_base_heap = base_heap->left_child;
    Node* cur_element_base_heap = base_heap->left_child;
    Node* cur_element_merging_heap = nullptr;
    if ((*merging_node).empty) {
      cur_element_merging_heap = merging_node->left_child;
      merging_node->left_child = nullptr;
    } else {
      cur_element_merging_heap = merging_node;
    }
    while (cur_element_merging_heap != nullptr) {
      if (cur_element_base_heap != nullptr &&
          cur_element_base_heap->degree < cur_element_merging_heap->degree) {
        before_element_base_heap = cur_element_base_heap;
        cur_element_base_heap = cur_element_base_heap->right_neighbour;
      } else if (cur_element_base_heap != nullptr &&
                 cur_element_base_heap->degree ==
                     cur_element_merging_heap->degree) {
        if (before_element_base_heap != cur_element_base_heap) {
          before_element_base_heap->right_neighbour =
              cur_element_base_heap->right_neighbour;
          ReHung(cur_element_merging_heap, cur_element_base_heap);
          cur_element_base_heap = before_element_base_heap->right_neighbour;
        } else {
          base_heap->left_child = before_element_base_heap->right_neighbour;
          before_element_base_heap = before_element_base_heap->right_neighbour;
          ReHung(cur_element_merging_heap, cur_element_base_heap);
          cur_element_base_heap = before_element_base_heap;
        }

        Node* blank;
        while (cur_element_merging_heap->right_neighbour != nullptr &&
               cur_element_merging_heap->degree ==
                   cur_element_merging_heap->right_neighbour->degree) {
          blank = cur_element_merging_heap->right_neighbour;
          ReHung(blank, cur_element_merging_heap);
          cur_element_merging_heap = blank;
        }
      } else {
        Node* next_elem = cur_element_merging_heap->right_neighbour;
        cur_element_merging_heap->parent = base_heap;

        if (before_element_base_heap == cur_element_base_heap) {
          base_heap->left_child = cur_element_merging_heap;
        } else {
          before_element_base_heap->right_neighbour = cur_element_merging_heap;
        }

        cur_element_merging_heap->right_neighbour = cur_element_base_heap;

        before_element_base_heap = cur_element_merging_heap;
        cur_element_merging_heap = next_elem;
      }
    }
  }

  int pointer_to_of_trash_;
  int** minimum_elements_;
  Node** heap_list_;
  Node* trash_;
  int* global_map_;

 public:
  BinomialHeap(int n, int q)
      : pointer_to_of_trash_(n),
        minimum_elements_(new int*[cHeapsCount]),
        heap_list_(new Node*[cHeapsCount]),
        trash_(new Node[cArraySize]),
        global_map_(new int[cArraySize]) {
    Node blank;
    for (int i = 0; i < n + q + 1; i++) {
      trash_[i] = blank;
      global_map_[i] = i;
      blank.great_num = i + 1;
    }
    for (int i = 0; i < n; i++) {
      heap_list_[i] = &trash_[i];
    }
    for (int i = 0; i < n; i++) {
      minimum_elements_[i] = &global_map_[i];
    }
  }
  ~BinomialHeap() {
    delete[] global_map_;
    delete[] trash_;
    delete[] minimum_elements_;
    delete[] heap_list_;
  }

  void SmartSwap(Node* a, Node* b) {
    std::swap(global_map_[a->great_num], global_map_[b->great_num]);
    std::swap(a->value, b->value);
    std::swap(a->great_num, b->great_num);
  }

  void SiftUp(Node* node) {
    while (node->parent != nullptr && !node->parent->empty &&
           node->value < node->parent->value) {
      SmartSwap(node, node->parent);
      node = node->parent;
    }
  }

  void AddNode(int value, int heap_num) {
    Node new_node;
    new_node.value = value;
    new_node.great_num = pointer_to_of_trash_;
    new_node.empty = false;
    trash_[pointer_to_of_trash_] = new_node;
    global_map_[pointer_to_of_trash_] = pointer_to_of_trash_;

    Merge(heap_list_[heap_num], &trash_[pointer_to_of_trash_]);
    if (trash_[*minimum_elements_[heap_num]].value > value) {
      minimum_elements_[heap_num] = &global_map_[pointer_to_of_trash_];
    }
    pointer_to_of_trash_++;
  }

  void MergeHeaps(int num_heap_a, int num_heap_b) {
    if (trash_[*minimum_elements_[num_heap_b]].value >
            trash_[*minimum_elements_[num_heap_a]].value ||
        (trash_[*minimum_elements_[num_heap_b]].value ==
             trash_[*minimum_elements_[num_heap_a]].value &&
         trash_[*minimum_elements_[num_heap_b]].great_num >
             trash_[*minimum_elements_[num_heap_a]].great_num)) {
      minimum_elements_[num_heap_b] = minimum_elements_[num_heap_a];
    }
    minimum_elements_[num_heap_a] =
        &global_map_[heap_list_[num_heap_a]->great_num];
    Merge(heap_list_[num_heap_b], heap_list_[num_heap_a]);
  }

  void ExtractMin(int num_heap) {
    Node* min_element = &trash_[*minimum_elements_[num_heap]];
    if (min_element == heap_list_[num_heap]->left_child) {
      heap_list_[num_heap]->left_child = min_element->right_neighbour;
    } else {
      Node* child = heap_list_[num_heap]->left_child;
      while (child->right_neighbour != min_element) {
        child = child->right_neighbour;
      }
      if (min_element != nullptr) {
        child->right_neighbour = min_element->right_neighbour;
      }
    }

    if (min_element != nullptr) {
      min_element->empty = true;
      ReverseChilds(min_element);
    }
    Merge(heap_list_[num_heap], min_element);

    minimum_elements_[num_heap] = &global_map_[heap_list_[num_heap]->great_num];
    Node* cur_element = heap_list_[num_heap]->left_child;
    while (cur_element != nullptr) {
      if (trash_[*minimum_elements_[num_heap]].value > cur_element->value ||
          (trash_[*minimum_elements_[num_heap]].value == cur_element->value &&
           cur_element->great_num <
               trash_[*minimum_elements_[num_heap]].great_num)) {
        minimum_elements_[num_heap] = &global_map_[cur_element->great_num];
      }
      cur_element = cur_element->right_neighbour;
    }
  }

  Node* DeleteNode(int index) {
    Node* parent = &trash_[global_map_[index]];
    while (!parent->empty) {
      parent = parent->parent;
    }
    trash_[global_map_[index]].value = -1;
    SiftUp(&trash_[global_map_[index]]);
    minimum_elements_[parent->great_num] = &global_map_[index];
    ExtractMin(parent->great_num);
    return parent;
  }

  void AssignNewValToNode(int value, int index) {
    Node* parent = DeleteNode(index);
    Node* new_node = &trash_[global_map_[index]];
    new_node->value = value;
    new_node->empty = false;
    new_node->right_neighbour = nullptr;
    new_node->parent = nullptr;
    new_node->left_child = nullptr;
    new_node->degree = 0;
    Merge(parent, new_node);
    if (trash_[*minimum_elements_[parent->great_num]].value > value ||
        (trash_[*minimum_elements_[parent->great_num]].value == value &&
         new_node->great_num <
             trash_[*minimum_elements_[parent->great_num]].great_num)) {
      minimum_elements_[parent->great_num] = &global_map_[index];
    }
    if (trash_[global_map_[index]].value == cInfty) {
      if (parent->left_child != nullptr) {
        std::cout << 1;
        exit(0);
      }
    }
  }

  int GetMin(int num_heap) {
    return trash_[*minimum_elements_[num_heap]].value;
  }
};

int main() {
  std::ios_base::sync_with_stdio(false);
  cin.tie(0);
  cout.tie(0);
  int n;
  int q;
  cin >> n >> q;

  BinomialHeap heap(n, q);

  while ((q--) != 0) {
    int code;
    cin >> code;
    if (code == 0) {
      int heap_num;
      int value;
      cin >> heap_num >> value;
      heap.AddNode(value, heap_num - 1);
    } else if (code == 1) {
      int heap_num_a;
      int heap_num_b;
      cin >> heap_num_a >> heap_num_b;
      heap.MergeHeaps(heap_num_a - 1, heap_num_b - 1);
    } else if (code == 2) {
      int ind;
      cin >> ind;
      heap.DeleteNode(ind + n - 1);
    } else if (code == 3) {
      int ind;
      int value;
      cin >> ind >> value;
      heap.AssignNewValToNode(value, ind + n - 1);
    } else if (code == 4) {
      int heap_num_a;
      cin >> heap_num_a;
      std::cout << heap.GetMin(heap_num_a - 1) << '\n';
    } else {
      int heap_num_a;
      cin >> heap_num_a;
      heap.ExtractMin(heap_num_a - 1);
    }
  }
  return 0;
}