#include "regex2eps_nfa.hpp"

/**
 * @struct NfaFragment
 * @brief Represents a fragment of an NFA with a single start and accept state. Used in Thompson's algorithm.
 */
struct NfaFragment {
    Vertex start;
    Vertex accept;
};


bool isOperator(char c) {
    return c == '|' || c == '*' || c == '+' || c == '.';
}


bool isOperand(char c) {
    return std::isalnum(c) || c == replace_eps[0];
}


/**
 * @brief Preprocesses a raw regex string by replacing epsilon, removing whitespace,
 *        and inserting explicit concatenation operators ('.').
 * @param raw_infix The raw regular expression string.
 * @return The preprocessed regex string ready for infix-to-postfix conversion.
 */
string preprocessRegex(string raw_infix) {
    // === Step 1: (replace eps) ===
    size_t pos = 0;
    while ((pos = raw_infix.find(epsilon_utf8, pos)) != string::npos) {
        raw_infix.replace(pos, epsilon_utf8.length(), replace_eps);
        pos += 1;
    }

    // === Step 2: Strip  ===
    
    string infix;
    for (char c : raw_infix) {
        if (!std::isspace(c)) {
            infix += c;
        }
    }
    // === Step 3: Insertion of an explicit concatenation operator '.' ===
    string result;
    if (infix.empty()) return result;
    result += infix[0]; // Add the first character without checks.

    for (size_t i = 1; i < infix.length(); ++i) {
        char prev = infix[i-1];
        char curr = infix[i];

        // Condition for inserting a dot:
        // 1. An operand followed by another operand: a b -> a.b | e a -> e.a
        // 2. A closing parenthesis followed by an operand: )a -> ).a
        // 3. A unary operator (* or +) followed by an operand: *a -> *.a
        // 4. An operand followed by an opening parenthesis: a( -> a.(
        // 5. A closing parenthesis followed by an opening parenthesis: )( -> ).(
        // 6. A unary operator followed by an opening parenthesis: *( -> *.( 
        if ((isOperand(prev) && isOperand(curr)) ||
            (prev == ')' && isOperand(curr)) ||
            ((prev == '*' || prev == '+') && isOperand(curr)) ||
            (isOperand(prev) && curr == '(') ||
            (prev == ')' && curr == '(') ||
            ((prev == '*' || prev == '+') && curr == '(')
           ) {
            result += '.';
        }
        result += curr;
    }

    return result;
}


/**
 * @brief Converts an infix regular expression to its postfix equivalent using the Shunting-yard algorithm.
 * @param infix The preprocessed infix regex string.
 * @return The postfix regex string.
 * @throws runtime_error on mismatched parentheses.
 */
string infixToPostfix(const string& infix) {
    if (debug) {
        std::cerr << infix << '\n';
    }
    map<char, int> precedence = {{'|', 1}, {'.', 2}, {'*', 3}, {'+', 3}};
    string postfix;
    std::stack<char> ops;

    for (char token : infix) {
        if (isOperand(token)) {
            postfix += token;
        } else if (token == '(') {
            ops.push(token);
        } else if (token == ')') {
            while (!ops.empty() && ops.top() != '(') {
                postfix += ops.top();
                ops.pop();
            }
            if (ops.empty()) throw runtime_error("Mismatched parentheses");
            ops.pop();
        } else {
            while (!ops.empty() && ops.top() != '(' && isOperator(ops.top()) && precedence[ops.top()] >= precedence[token]) {
                postfix += ops.top();
                ops.pop();
            }
            ops.push(token);
        }
    }

    while (!ops.empty()) {
        if (ops.top() == '(') throw runtime_error("Mismatched parentheses");
        postfix += ops.top();
        ops.pop();
    }

    return postfix;
}


/**
 * @brief Constructs an epsilon-NFA from a postfix regular expression using Thompson's construction algorithm.
 * @param postfix The postfix regex string.
 * @return A Graph representing the resulting epsilon-NFA.
 * @throws runtime_error on invalid postfix expressions.
 */
Graph buildNfa(const string& postfix) {
    if (debug) {
        std::cerr << postfix << '\n';
    }
    Graph g;
    std::stack<NfaFragment> fragments;
    int state_counter = 0;

    auto add_epsilon_edge = [&](Vertex u, Vertex v) {
        EdgeProperties props = {1.0, std::nullopt};
        boost::add_edge(u, v, props, g);
    };

    auto add_default_vertex = [&]() -> Vertex {
        return boost::add_vertex(VertexProperties{state_counter++, VertexType::Default}, g);
    };

    for (char token : postfix) {
        if (token == replace_eps[0]) {
            Vertex start = add_default_vertex();
            Vertex accept = add_default_vertex();
            add_epsilon_edge(start, accept);
            fragments.push({start, accept});
        } else if (isOperand(token)) {
            Vertex start = add_default_vertex();
            Vertex accept = add_default_vertex();
            
            EdgeProperties props = {1.0, token};
            boost::add_edge(start, accept, props, g);
            
            fragments.push({start, accept});
        } else {
            Vertex new_start, new_accept;
            NfaFragment frag, frag_a, frag_b;

            switch (token) {
                case '.':
                    if (fragments.size() < 2) throw runtime_error("Invalid postfix for '.'");
                    frag_b = fragments.top(); fragments.pop();
                    frag_a = fragments.top(); fragments.pop();
                    add_epsilon_edge(frag_a.accept, frag_b.start);
                    fragments.push({frag_a.start, frag_b.accept});
                    break;

                case '|':
                    if (fragments.size() < 2) throw runtime_error("Invalid postfix for '|'");
                    frag_b = fragments.top(); fragments.pop();
                    frag_a = fragments.top(); fragments.pop();
                    new_start = add_default_vertex();
                    new_accept = add_default_vertex();
                    add_epsilon_edge(new_start, frag_a.start);
                    add_epsilon_edge(new_start, frag_b.start);
                    add_epsilon_edge(frag_a.accept, new_accept);
                    add_epsilon_edge(frag_b.accept, new_accept);
                    fragments.push({new_start, new_accept});
                    break;

                case '*':
                    if (fragments.empty()) throw runtime_error("Invalid postfix for '*'");
                    frag = fragments.top(); fragments.pop();
                    new_start = add_default_vertex();
                    new_accept = add_default_vertex();
                    add_epsilon_edge(new_start, new_accept);
                    add_epsilon_edge(new_start, frag.start);
                    add_epsilon_edge(frag.accept, new_accept);
                    add_epsilon_edge(frag.accept, frag.start);
                    fragments.push({new_start, new_accept});
                    break;
                
                case '+':
                    if (fragments.empty()) throw runtime_error("Invalid postfix for '+'");
                    frag = fragments.top(); fragments.pop();
                    new_start = add_default_vertex();
                    new_accept = add_default_vertex();
                    add_epsilon_edge(new_start, frag.start);
                    add_epsilon_edge(frag.accept, new_accept);
                    add_epsilon_edge(frag.accept, frag.start);
                    fragments.push({new_start, new_accept});
                    break;
            }
        }
    }

    if (fragments.size() != 1) {
        throw runtime_error("Final stack should contain one fragment");
    }

    NfaFragment final_fragment = fragments.top();
    
    g[final_fragment.start].type = VertexType::Start;
    g[final_fragment.accept].type = VertexType::Terminal;

    return g;
}


/**
 * @brief Parses a regex string and converts it into an epsilon-NFA graph.
 * @param regex The input regular expression.
 * @return A Graph representing the corresponding epsilon-NFA.
 */
Graph parseRegex (const string& regex) {
    string processed_regex = preprocessRegex(regex);

    string postfix_regex = infixToPostfix(processed_regex);
    return buildNfa(postfix_regex);
}
