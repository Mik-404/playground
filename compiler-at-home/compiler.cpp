#include "compiler.h"
#include <string>
#include <stdint.h>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <iomanip>
#include <bitset>
#include <unordered_map>

const uint32_t CONDITIONAL = 0b1110;
const uint32_t MOVW = 0b00110000;
const uint32_t MOVT = 0b00110100;
const uint32_t MOV = 0b0001101000000000000000000000;
const uint32_t LDR = 0b01010001;
const uint32_t STR = 0b01010010;
const uint32_t BLX = 0b0001001011111111111100110000;
const uint32_t SMUL = 0b0001011000000000000010000000;
const uint32_t ADD = 0b0000100000000000000000000000;
const uint32_t SUB = 0b0000010000000000000000000000;
const uint32_t BX_LR = 0b11100001001011111111111100011110;

std::unordered_map<std::string, void*> table;

size_t global_ptr = 0;

struct Operator {
    virtual void compute (uint8_t reg_dest, uint8_t reg_n, uint8_t reg_m, void*& addr) = 0;
};

struct AddOp : Operator {
    virtual void compute (uint8_t reg_dest, uint8_t reg_n, uint8_t reg_m, void*& addr) override {
        uint32_t opcode_ = (CONDITIONAL << 28) | ADD | (reg_n << 16) | (reg_dest << 12) | (reg_m);
        uint32_t* current_opcode_ptr = static_cast<uint32_t*>(addr);
        *(current_opcode_ptr) = opcode_;
        current_opcode_ptr += 1;

        addr = current_opcode_ptr;
    }
};

struct SubOp : Operator {
    virtual void compute (uint8_t reg_dest, uint8_t reg_n, uint8_t reg_m, void*& addr) override {
        uint32_t opcode_ = (CONDITIONAL << 28) | SUB | (reg_n << 16) | (reg_dest << 12) | (reg_m);
        uint32_t* current_opcode_ptr = static_cast<uint32_t*>(addr);
        *(current_opcode_ptr) = opcode_;
        current_opcode_ptr += 1;

        addr = current_opcode_ptr;
    }
};

struct MulOp : Operator {
    virtual void compute (uint8_t reg_dest, uint8_t reg_n, uint8_t reg_m, void*& addr) override {
        uint32_t opcode_ = (CONDITIONAL << 28) | SMUL | (reg_dest << 16) | (reg_m << 8) | (reg_n);
        uint32_t* current_opcode_ptr = static_cast<uint32_t*>(addr);
        *(current_opcode_ptr) = opcode_;
        current_opcode_ptr += 1;

        addr = current_opcode_ptr;
    }
};

struct literal_group {

    std::vector <literal_group*> childs;

    static const uint8_t sp = 0b1101;

    static void mov_to_register (uint8_t reg_num_target, uint8_t reg_num_source, void*& addr) {
        uint32_t opcode_ = (CONDITIONAL << 28) | MOV | (reg_num_target << 12) | reg_num_source;
        uint32_t* current_opcode_ptr = static_cast<uint32_t*>(addr);
        *(current_opcode_ptr) = opcode_;
        current_opcode_ptr += 1;

        addr = current_opcode_ptr;
    }

    static void load_to_register (uint8_t reg_num_target, uint8_t reg_num_source, uint16_t shift, uint8_t sign, void*& addr) {
        uint32_t opcode_ldr = ((((((CONDITIONAL << 8) | LDR | (sign << 3)) << 4) | reg_num_source) << 4) | reg_num_target) << 12;
        uint32_t* current_opcode_ptr = static_cast<uint32_t*>(addr);
        *(current_opcode_ptr) = opcode_ldr;
        current_opcode_ptr += 1;

        addr = current_opcode_ptr;
    }

    static void pop_from_stack (uint8_t reg_num, void*& addr) {
        uint32_t LDR_with_post_inc = (LDR | 0b1000) & 0b11101111;
        uint32_t opcode_ = (((((((CONDITIONAL << 8) | LDR_with_post_inc) << 4) | sp) << 4) | reg_num) << 12) | 0b100;
        uint32_t* current_opcode_ptr = static_cast<uint32_t*>(addr);
        *(current_opcode_ptr) = opcode_;
        current_opcode_ptr += 1;

        addr = current_opcode_ptr;
    }

    static void push_to_stack (uint8_t reg_num, void*& addr) {
        uint32_t opcode_ = (((((((CONDITIONAL << 8) | STR) << 4) | sp) << 4) | reg_num) << 12) | 0b100;
        uint32_t* current_opcode_ptr = static_cast<uint32_t*>(addr);
        *(current_opcode_ptr) = opcode_;
        current_opcode_ptr += 1;

        addr = current_opcode_ptr;
    }

    static void put_ptr_to_register (uint8_t reg_num, void* ptr, void*& addr) {
        uintptr_t ptr_to_data = (uintptr_t)ptr;
        uintptr_t left_part = ptr_to_data >> 16;
        uintptr_t right_part = ptr_to_data & 0x0FFFF;

        uint32_t opcode_movw = (((((((CONDITIONAL << 8) | MOVW) << 4) | (right_part >> 12)) << 4) | reg_num) << 12) | (right_part & 0x0FFF);
        uint32_t opcode_movt = (((((((CONDITIONAL << 8) | MOVT) << 4) | (left_part >> 12)) << 4) | reg_num) << 12) | (left_part & 0x0FFF);

        uint32_t* current_opcode_ptr = static_cast<uint32_t*>(addr);
        *(current_opcode_ptr) = opcode_movw;
        current_opcode_ptr += 1;

        *(current_opcode_ptr) = opcode_movt;
        current_opcode_ptr += 1;

        addr = current_opcode_ptr;
    }

    virtual void put_into_register (uint8_t reg_num, void*& addr) = 0;


};

struct Var : literal_group {

    void* var_data_addr;

    Var(void* addr): var_data_addr(addr) {}

    void put_into_register (uint8_t reg_num, void*& addr) override {
        put_ptr_to_register (reg_num, var_data_addr, addr);
        load_to_register(reg_num, reg_num, 0, 1, addr);
    }

};

struct Num : literal_group {

    int16_t value;

    Num(int16_t value): value(value) {}

    void put_into_register (uint8_t reg_num, void*& addr) override {
        uint32_t opcode = (((((((CONDITIONAL << 8) | MOVW) << 4) | (value >> 12)) << 4) | reg_num) << 12) | (value & 0x0FFF);
        uint32_t* current_opcode_ptr = static_cast<uint32_t*>(addr);
        *(current_opcode_ptr) = opcode;
        current_opcode_ptr += 1;
        addr = current_opcode_ptr;
    }

};


struct Func : literal_group {
    
    void* text_addr;

    Func(void* addr, std::string_view expr);
    ~Func();
    void put_into_register (uint8_t reg_num, void*& addr) override;

};

struct Monom : literal_group {
    Monom (std::string_view expr);
    ~Monom();
    void put_into_register (uint8_t reg_num, void*& addr) override;
};

struct Polynom : literal_group {

    Operator* op = nullptr;

    Polynom (std::string_view expr);
    ~Polynom();
    void put_into_register (uint8_t reg_num, void*& addr) override;

};


Func::Func(void* addr, std::string_view expr): text_addr(addr) {
    size_t pos = 0;
    if (expr[0] == ')') {
        global_ptr = pos;
        return;
    }
    while (true) {
        childs.push_back(new Polynom(expr.substr(pos, expr.size() - (pos))));
        pos += global_ptr;
        if (expr[pos + 1] == ')') break;
        pos += 2;
    }
    global_ptr = pos;
}

Func::~Func() {
    for (size_t iter = 0; iter < childs.size(); iter++) {
        delete childs[iter];
    }
}

void Func::put_into_register (uint8_t reg_num, void*& addr) {
    for (auto child : childs) {
        child->put_into_register(0b11, addr);
        push_to_stack(0b11, addr);
        push_to_stack(0b11, addr); // выравнивание
    }
    for (int i = childs.size() - 1; i >= 0; i--) {
        pop_from_stack(i, addr);
        pop_from_stack(i, addr);
    }
    push_to_stack(0b1110, addr); //LR
    put_ptr_to_register(0b101, text_addr, addr);
    uint32_t opcode_ = (CONDITIONAL << 28) | BLX | 0b101;
    uint32_t* current_opcode_ptr = static_cast<uint32_t*>(addr);
    *(current_opcode_ptr) = opcode_;
    current_opcode_ptr += 1;

    addr = current_opcode_ptr;
    mov_to_register(reg_num, 0b0, addr);
    pop_from_stack(0b1110, addr); //LR
}

Monom::Monom (std::string_view expr) {
    size_t pos = 0;
    while (true) {
        if (expr[pos] == '(') {
            childs.push_back(new Polynom(expr.substr(pos + 1, expr.size() - (pos+1))));
            pos += global_ptr + 2;
            if (expr[pos] != ')') throw std::logic_error("brackets didn't parse correctly");
            if (pos + 1 >= expr.size()) break;
            if (expr[pos + 1] != '*') break;
            pos += 2;
        } else {
            std::string cur_item;
            for (;(std::isalnum(expr[pos]) || expr[pos] == '_') && pos < expr.size(); ++pos) {
                cur_item.push_back(expr[pos]);
            }
            if (table.find(cur_item) != table.end()) {
                void* addr = table[cur_item];
                if (pos == expr.size() || expr[pos] != '(') {
                    childs.push_back(new Var(addr));
                    pos--;
                } else {
                    std::string_view loc_expr = expr.substr(pos + 1, expr.size() - (pos + 1));
                    childs.push_back(new Func(addr, loc_expr));
                    if (expr[pos + 1] == ')') {
                        pos += global_ptr + 1;
                    } else {
                        pos += global_ptr + 2;
                    }
                }
            } else {
                int16_t val = std::stoi(cur_item);
                childs.push_back(new Num(val));
                pos--;
            }
            if (pos + 1 == expr.size()) {
                break;
            }
            if (expr[pos + 1] != '*') break;
            pos+=2;
        }
    }
    global_ptr = pos;
}

Monom::~Monom() {
    for (size_t iter = 0; iter < childs.size(); iter++) {
        delete childs[iter];
    }
}

void Monom::put_into_register (uint8_t reg_num, void*& addr) {
    for (auto child : childs) {
        child->put_into_register(0b11, addr);
        push_to_stack(0b11, addr); // выравнивание
        push_to_stack(0b11, addr);
    }
    MulOp mult_;
    pop_from_stack(0, addr);
    pop_from_stack(0, addr);
    for (uint32_t i = 1; i < childs.size(); i++) {
        pop_from_stack(0b1, addr);
        pop_from_stack(0b1, addr);
        mult_.compute(0b0, 0b0, 0b1, addr);
    }
    mov_to_register(reg_num, 0b0, addr);
}


Polynom::Polynom (std::string_view expr) {
    if (expr.empty()) throw std::logic_error("empty polynom");

    if (expr[0] == '-') {
        childs.push_back(new Num(0));
        op = new SubOp();
        childs.push_back(new Polynom(expr.substr(1)));
        global_ptr = 1 + global_ptr;
        return;
    }

    childs.push_back(new Monom(expr));
    if (global_ptr + 1 == expr.size()) {
        return;
    }
    global_ptr++; // goto op
    if (expr[global_ptr] == '+') {
        op = new AddOp();
    } else if (expr[global_ptr] == '-') {
        op = new SubOp();
    } else {
        global_ptr--;
        return;
    }
    size_t fix_global_ptr = global_ptr + 1;
    childs.push_back(new Polynom(expr.substr(fix_global_ptr, expr.size() - (fix_global_ptr))));
    global_ptr += fix_global_ptr;

}

Polynom::~Polynom() {
    if (op != nullptr) delete op;
    for (size_t iter = 0; iter < childs.size(); iter++) {
        delete childs[iter];
    }
}

void Polynom::put_into_register (uint8_t reg_num, void*& addr) {
    if (op == nullptr) {
        childs.at(0)->put_into_register(reg_num, addr);
        return;
    }
    childs.at(0)->put_into_register(0b0, addr);
    push_to_stack(0b0, addr);
    push_to_stack(0b0, addr); // чтоб стек был выровнен
    childs.at(1)->put_into_register(0b1, addr);
    pop_from_stack(0b0, addr);
    pop_from_stack(0b0, addr);

    op->compute(reg_num, 0b0, 0b1, addr);
}


int compile(const char *expression, const symbol_t **symbol_table, void *opcodes_out) {
    std::string expr = expression;
    std::string expr_no_spaces;
    uint32_t* opcodes_out_deb = (uint32_t*)opcodes_out; 
    for (auto i : expr) {
        if (!std::isspace(i)) expr_no_spaces.push_back(i);
    }
    for (uint32_t iter = 0; symbol_table[iter] != NULL; iter++) {
        table[symbol_table[iter]->name] = symbol_table[iter]->addr;
    }
    literal_group* all_expr = new Polynom (expr_no_spaces);
    all_expr->put_into_register(0b0, opcodes_out);

    uint32_t* return_ptr = static_cast<uint32_t*>(opcodes_out);
    *return_ptr = BX_LR;
    return_ptr++;
    opcodes_out = return_ptr;

    delete all_expr;
    return 0;
}

