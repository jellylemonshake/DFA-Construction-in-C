#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_STATES 100
#define MAX_SYMBOLS 26
#define MAX_NODES 200

typedef enum { CHAR_NODE, CONCAT_NODE, OR_NODE, STAR_NODE } NodeType;

typedef struct Node {
    NodeType type;
    char symbol;
    int position;
    int nullable;
    int firstpos[MAX_NODES];
    int lastpos[MAX_NODES];
    int firstpos_count;
    int lastpos_count;
    struct Node *left;
    struct Node *right;
} Node;

int position_counter = 0;
int followpos[MAX_NODES][MAX_NODES];
int followpos_count[MAX_NODES];
char position_symbol[MAX_NODES];

typedef struct {
    int positions[MAX_NODES];
    int count;
    int is_final;
} DFAState;

DFAState dfa_states[MAX_STATES];
int dfa_transition[MAX_STATES][MAX_SYMBOLS];
int num_dfa_states = 0;

// Functions
Node* create_node(NodeType type, char symbol, Node* left, Node* right);
Node* parse_regex(char* regex, int* idx);
Node* parse_term(char* regex, int* idx);
Node* parse_factor(char* regex, int* idx);
void compute_functions(Node* node);
void compute_followpos(Node* node);
void print_syntax_tree(Node* node, int level);
void print_functions_table(Node* node);
int add_to_set(int* set, int* count, int value);
int sets_equal(int* set1, int count1, int* set2, int count2);
int find_dfa_state(int* positions, int count);
void construct_dfa(Node* root);
void print_dfa();
int simulate_dfa(char* input);

Node* create_node(NodeType type, char symbol, Node* left, Node* right) {
    Node* node = (Node*)malloc(sizeof(Node));
    node->type = type;
    node->symbol = symbol;
    node->left = left;
    node->right = right;
    node->firstpos_count = 0;
    node->lastpos_count = 0;
    node->nullable = 0;
    
    if (type == CHAR_NODE) {
        node->position = ++position_counter;
        position_symbol[node->position] = symbol;
    } else {
        node->position = 0;
    }
    
    return node;
}

Node* parse_regex(char* regex, int* idx) {
    Node* left = parse_term(regex, idx);
    
    while (regex[*idx] == '|') {
        (*idx)++;
        Node* right = parse_term(regex, idx);
        left = create_node(OR_NODE, '|', left, right);
    }
    
    return left;
}

Node* parse_term(char* regex, int* idx) {
    Node* left = parse_factor(regex, idx);
    
    while (regex[*idx] && regex[*idx] != ')' && regex[*idx] != '|') {
        Node* right = parse_factor(regex, idx);
        left = create_node(CONCAT_NODE, '.', left, right);
    }
    
    return left;
}

Node* parse_factor(char* regex, int* idx) {
    Node* node = NULL;
    
    if (regex[*idx] == '(') {
        (*idx)++;
        node = parse_regex(regex, idx);
        if (regex[*idx] == ')') (*idx)++;
    } else {
        node = create_node(CHAR_NODE, regex[*idx], NULL, NULL);
        (*idx)++;
    }
    
    if (regex[*idx] == '*') {
        node = create_node(STAR_NODE, '*', node, NULL);
        (*idx)++;
    }
    
    return node;
}

int add_to_set(int* set, int* count, int value) {
    for (int i = 0; i < *count; i++) {
        if (set[i] == value) return 0;
    }
    set[(*count)++] = value;
    return 1;
}

void compute_functions(Node* node) {
    if (!node) return;
    
    compute_functions(node->left);
    compute_functions(node->right);
    
    switch (node->type) {
        case CHAR_NODE:
            node->nullable = 0;
            add_to_set(node->firstpos, &node->firstpos_count, node->position);
            add_to_set(node->lastpos, &node->lastpos_count, node->position);
            break;
            
        case OR_NODE:
            node->nullable = node->left->nullable || node->right->nullable;
            for (int i = 0; i < node->left->firstpos_count; i++)
                add_to_set(node->firstpos, &node->firstpos_count, node->left->firstpos[i]);
            for (int i = 0; i < node->right->firstpos_count; i++)
                add_to_set(node->firstpos, &node->firstpos_count, node->right->firstpos[i]);
            for (int i = 0; i < node->left->lastpos_count; i++)
                add_to_set(node->lastpos, &node->lastpos_count, node->left->lastpos[i]);
            for (int i = 0; i < node->right->lastpos_count; i++)
                add_to_set(node->lastpos, &node->lastpos_count, node->right->lastpos[i]);
            break;
            
        case CONCAT_NODE:
            node->nullable = node->left->nullable && node->right->nullable;
            if (node->left->nullable) {
                for (int i = 0; i < node->left->firstpos_count; i++)
                    add_to_set(node->firstpos, &node->firstpos_count, node->left->firstpos[i]);
                for (int i = 0; i < node->right->firstpos_count; i++)
                    add_to_set(node->firstpos, &node->firstpos_count, node->right->firstpos[i]);
            } else {
                for (int i = 0; i < node->left->firstpos_count; i++)
                    add_to_set(node->firstpos, &node->firstpos_count, node->left->firstpos[i]);
            }
            if (node->right->nullable) {
                for (int i = 0; i < node->left->lastpos_count; i++)
                    add_to_set(node->lastpos, &node->lastpos_count, node->left->lastpos[i]);
                for (int i = 0; i < node->right->lastpos_count; i++)
                    add_to_set(node->lastpos, &node->lastpos_count, node->right->lastpos[i]);
            } else {
                for (int i = 0; i < node->right->lastpos_count; i++)
                    add_to_set(node->lastpos, &node->lastpos_count, node->right->lastpos[i]);
            }
            break;
            
        case STAR_NODE:
            node->nullable = 1;
            for (int i = 0; i < node->left->firstpos_count; i++)
                add_to_set(node->firstpos, &node->firstpos_count, node->left->firstpos[i]);
            for (int i = 0; i < node->left->lastpos_count; i++)
                add_to_set(node->lastpos, &node->lastpos_count, node->left->lastpos[i]);
            break;
    }
}

void compute_followpos(Node* node) {
    if (!node) return;
    
    if (node->type == CONCAT_NODE) {
        for (int i = 0; i < node->left->lastpos_count; i++) {
            int pos = node->left->lastpos[i];
            for (int j = 0; j < node->right->firstpos_count; j++) {
                add_to_set(followpos[pos], &followpos_count[pos], node->right->firstpos[j]);
            }
        }
    } else if (node->type == STAR_NODE) {
        for (int i = 0; i < node->lastpos_count; i++) {
            int pos = node->lastpos[i];
            for (int j = 0; j < node->firstpos_count; j++) {
                add_to_set(followpos[pos], &followpos_count[pos], node->firstpos[j]);
            }
        }
    }
    
    compute_followpos(node->left);
    compute_followpos(node->right);
}

void print_syntax_tree(Node* node, int level) {
    if (!node) return;
    
    for (int i = 0; i < level; i++) printf("  ");
    
    switch (node->type) {
        case CHAR_NODE:
            printf("'%c' (pos: %d)\n", node->symbol, node->position);
            break;
        case OR_NODE:
            printf("OR\n");
            break;
        case CONCAT_NODE:
            printf("CONCAT\n");
            break;
        case STAR_NODE:
            printf("STAR\n");
            break;
    }
    
    print_syntax_tree(node->left, level + 1);
    print_syntax_tree(node->right, level + 1);
}

// Print functions table
void print_functions_table(Node* node) {
    printf("\nFUNCTIONS TABLE\n");
    printf("%-8s %-10s %-8s %-20s %-20s\n", "Position", "Symbol", "Nullable", "Firstpos", "Lastpos");
    
    for (int i = 1; i <= position_counter; i++) {
        printf("%-8d %-10c %-8s ", i, position_symbol[i], "false");
        
        printf("{");
        printf("%d", i);
        printf("}");
        
        for (int j = 0; j < 20 - 3; j++) printf(" ");
        
        printf("{");
        printf("%d", i);
        printf("}\n");
    }
    
    printf("\nFOLLOWPOS TABLE\n");
    printf("%-10s %-30s\n", "Position", "Followpos");
    
    for (int i = 1; i <= position_counter; i++) {
        printf("%-10d {", i);
        for (int j = 0; j < followpos_count[i]; j++) {
            printf("%d", followpos[i][j]);
            if (j < followpos_count[i] - 1) printf(", ");
        }
        printf("}\n");
    }
}

int sets_equal(int* set1, int count1, int* set2, int count2) {
    if (count1 != count2) return 0;
    for (int i = 0; i < count1; i++) {
        int found = 0;
        for (int j = 0; j < count2; j++) {
            if (set1[i] == set2[j]) {
                found = 1;
                break;
            }
        }
        if (!found) return 0;
    }
    return 1;
}

int find_dfa_state(int* positions, int count) {
    for (int i = 0; i < num_dfa_states; i++) {
        if (sets_equal(dfa_states[i].positions, dfa_states[i].count, positions, count))
            return i;
    }
    return -1;
}

void construct_dfa(Node* root) {
    // Initialize
    for (int i = 0; i < MAX_STATES; i++) {
        for (int j = 0; j < MAX_SYMBOLS; j++) {
            dfa_transition[i][j] = -1;
        }
    }
    
    // Start state = firstpos(root)
    for (int i = 0; i < root->firstpos_count; i++) {
        dfa_states[0].positions[i] = root->firstpos[i];
    }
    dfa_states[0].count = root->firstpos_count;
    dfa_states[0].is_final = 0;
    
    // Check if start state contains end marker position
    for (int i = 0; i < dfa_states[0].count; i++) {
        if (position_symbol[dfa_states[0].positions[i]] == '#') {
            dfa_states[0].is_final = 1;
            break;
        }
    }
    
    num_dfa_states = 1;
    
    int unmarked[MAX_STATES] = {0};
    unmarked[0] = 1;
    
    while (1) {
        int current = -1;
        for (int i = 0; i < num_dfa_states; i++) {
            if (unmarked[i]) {
                current = i;
                unmarked[i] = 0;
                break;
            }
        }
        
        if (current == -1) break;
        
        // For each input symbol
        for (char c = 'a'; c <= 'z'; c++) {
            int new_state_pos[MAX_NODES];
            int new_state_count = 0;
            
            // For each position in current state
            for (int i = 0; i < dfa_states[current].count; i++) {
                int pos = dfa_states[current].positions[i];
                if (position_symbol[pos] == c) {
                    // Add followpos(pos) to new state
                    for (int j = 0; j < followpos_count[pos]; j++) {
                        add_to_set(new_state_pos, &new_state_count, followpos[pos][j]);
                    }
                }
            }
            
            if (new_state_count > 0) {
                int state_idx = find_dfa_state(new_state_pos, new_state_count);
                
                if (state_idx == -1) {
                    // Create new state
                    state_idx = num_dfa_states++;
                    for (int i = 0; i < new_state_count; i++) {
                        dfa_states[state_idx].positions[i] = new_state_pos[i];
                    }
                    dfa_states[state_idx].count = new_state_count;
                    dfa_states[state_idx].is_final = 0;
                    
                    // Check if new state contains end marker
                    for (int i = 0; i < new_state_count; i++) {
                        if (position_symbol[new_state_pos[i]] == '#') {
                            dfa_states[state_idx].is_final = 1;
                            break;
                        }
                    }
                    
                    unmarked[state_idx] = 1;
                }
                
                dfa_transition[current][c - 'a'] = state_idx;
            }
        }
    }
}

void print_dfa() {
    printf("\nDFA STATES\n");
    for (int i = 0; i < num_dfa_states; i++) {
        printf("State %d: {", i);
        for (int j = 0; j < dfa_states[i].count; j++) {
            printf("%d", dfa_states[i].positions[j]);
            if (j < dfa_states[i].count - 1) printf(", ");
        }
        printf("} %s\n", dfa_states[i].is_final ? "[FINAL]" : "");
    }
    
    printf("\nDFA TRANSITIONS\n");
    printf("%-8s", "State");
    for (char c = 'a'; c <= 'z'; c++) {
        int has_transition = 0;
        for (int i = 0; i < num_dfa_states; i++) {
            if (dfa_transition[i][c - 'a'] != -1) {
                has_transition = 1;
                break;
            }
        }
        if (has_transition) printf(" %-5c", c);
    }
    printf("\n");
    
    for (int i = 0; i < num_dfa_states; i++) {
        printf("%-8d", i);
        for (char c = 'a'; c <= 'z'; c++) {
            int has_transition = 0;
            for (int j = 0; j < num_dfa_states; j++) {
                if (dfa_transition[j][c - 'a'] != -1) {
                    has_transition = 1;
                    break;
                }
            }
            if (has_transition) {
                if (dfa_transition[i][c - 'a'] != -1)
                    printf(" %-5d", dfa_transition[i][c - 'a']);
                else
                    printf(" %-5s", "-");
            }
        }
        printf("\n");
    }
}

int simulate_dfa(char* input) {
    int current_state = 0;
    
    printf("\nSimulating: \"%s\"\n", input);
    printf("State sequence: 0");
    
    for (int i = 0; input[i] != '\0'; i++) {
        char c = input[i];
        if (c < 'a' || c > 'z') {
            printf(" -> INVALID INPUT\n");
            return 0;
        }
        
        int next_state = dfa_transition[current_state][c - 'a'];
        if (next_state == -1) {
            printf(" -> REJECTED (no transition on '%c')\n", c);
            return 0;
        }
        
        current_state = next_state;
        printf(" -> %d", current_state);
    }
    
    if (dfa_states[current_state].is_final) {
        printf(" -> ACCEPTED\n");
        return 1;
    } else {
        printf(" -> REJECTED (not in final state)\n");
        return 0;
    }
}

int main() {
    char regex[200];
    char augmented[220];
    printf("  23BPS1146_Soham-Ghosh\n");
    printf("\n");
    printf("  DFA CONSTRUCTION USING DIRECT METHOD\n");
    printf("\n\n");
    
    printf("Enter regular expression: ");
    scanf("%s", regex);
    
    sprintf(augmented, "(%s)#", regex);
    printf("\nAugmented regex: %s\n", augmented);
    
    // Parse and build syntax tree
    int idx = 0;
    Node* root = parse_regex(augmented, &idx);
    
    printf("\nSYNTAX TREE\n");
    print_syntax_tree(root, 0);
    
    compute_functions(root);
    compute_followpos(root);
    
    print_functions_table(root);
    construct_dfa(root);
    print_dfa();
    printf("\nTESTING DFA\n");
    char test_input[100];
    
    while (1) {
        printf("\nEnter test string (or 'quit' to exit): ");
        scanf("%s", test_input);
        if (strcmp(test_input, "quit") == 0) break;
        simulate_dfa(test_input);
    }
    
    return 0;
}
