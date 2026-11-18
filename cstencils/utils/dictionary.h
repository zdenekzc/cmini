
/* dictionary.h */

#ifndef __PROG_DICTIONARY_H__
#define __PROG_DICTIONARY_H__

#include "std.h"
#include <map>

OPEN_NAMESPACE

/* Dictionary */

template <class Key, class Data>
class Dictionary
{
   private:
      typedef map <Key, Data> dict_type;
      typedef typename dict_type::iterator dict_iter;
      dict_type dict;

   public:
      bool contains (Key inx)          { return dict.find (inx) != dict.end (); }
      void store (Key inx, Data value) { dict [inx] = value; }
      void clear ()                    { dict.clear (); }
      void size ()                     { dict.size (); }

      Data get (Key inx)
      {
         dict_iter it = dict.find (inx);
         if (it == dict.end ())
            return Data ();
         else
            return it->second;
      }
};

template <typename Key, typename Data>
class Dictionary <Key *, Data>
{
   private:
      typedef map <size_t, Data> dict_type;
      typedef typename dict_type::iterator dict_iter;
      dict_type dict;

   public:
      bool contains (Key * inx)          { return dict.find (reinterpret_cast <size_t> (inx)) != dict.end (); }
      void store (Key * inx, Data value) { dict [reinterpret_cast <size_t> (inx)] = value; }
      void clear ()                      { dict.clear (); }

      Data get (Key * inx)
      {
         dict_iter it = dict.find (reinterpret_cast <size_t> (inx));
         if (dict.find (reinterpret_cast <size_t> (inx)) == dict.end ())
            return Data ();
         else
            return dict.find (reinterpret_cast <size_t> (inx))->second;
      }
};

#if 0
template <typename Data>
class Dictionary <string, Data>
{
   private:
      typedef map <string, Data> dict_type;
      typedef typename dict_type::iterator dict_iter;
      dict_type dict;

   public:
      bool contains (string inx)          { return dict.find (inx) != dict.end (); }
      void store (string inx, Data value) { dict [inx] = value; }
      void clear ()                       { dict.clear (); }

      Data get (string inx)
      {
         dict_iter it = dict.find (inx);
         if (it == dict.end ())
            return Data ();
         else
            return it->second;
      }
};
#endif

typedef Dictionary <string, string> StringDictionary;

/* ---------------------------------------------------------------------- */

#if USE_TRIE
template <typename Data>
class Dictionary<std::string, Data> {
    private:
        // Supporting the full 8-bit ASCII character set
        static const int ALPHABET_SIZE = 256;

        struct TrieNode {
            TrieNode* children[ALPHABET_SIZE];
            bool is_end_of_word;
            Data value;

            TrieNode() : is_end_of_word(false), value(Data()) {
                for (int i = 0; i < ALPHABET_SIZE; i++) {
                    children[i] = nullptr;
                }
            }
        };

        TrieNode* root;
        size_t current_size;

        // Helper to safely convert char to an index within 0-255 range
        int get_index(char c) const {
            return static_cast<unsigned char>(c);
        }

        // Recursive helper to free dynamically allocated memory
        void clear_node(TrieNode* node) {
            if (!node) return;
            for (int i = 0; i < ALPHABET_SIZE; i++) {
                if (node->children[i]) {
                    clear_node(node->children[i]);
                }
            }
            delete node;
        }

        // Recursive helper for deep copying another Trie
        TrieNode* clone_node(TrieNode* other_node) {
            if (!other_node) return nullptr;

            TrieNode* new_node = new TrieNode();
            new_node->is_end_of_word = other_node->is_end_of_word;
            new_node->value = other_node->value;

            for (int i = 0; i < ALPHABET_SIZE; i++) {
                if (other_node->children[i]) {
                    new_node->children[i] = clone_node(other_node->children[i]);
                }
            }
            return new_node;
        }

    public:
        // Default Constructor
        Dictionary() : root(new TrieNode()), current_size(0) {}

        // Destructor - handles memory cleanup
        ~Dictionary() {
            clear_node(root);
        }

        // Copy Constructor - performs a deep copy of the tree
        Dictionary(const Dictionary& other) {
            root = clone_node(other.root);
            current_size = other.current_size;
        }

        // Copy Assignment Operator - prevents memory leaks during assignment
        Dictionary& operator=(const Dictionary& other) {
            if (this != &other) {
                TrieNode* new_root = clone_node(other.root);
                clear_node(root);
                root = new_root;
                current_size = other.current_size;
            }
            return *this;
        }

        // Checks if the key exists in the dictionary
        bool contains(const std::string& inx) const {
            TrieNode* current = root;
            for (char c : inx) {
                int index = get_index(c);
                if (!current->children[index]) {
                    return false;
                }
                current = current->children[index];
            }
            return current->is_end_of_word;
        }

        // Inserts or updates a value associated with the key
        void store(const std::string& inx, const Data& value) {
            TrieNode* current = root;
            for (char c : inx) {
                int index = get_index(c);
                if (!current->children[index]) {
                    current->children[index] = new TrieNode();
                }
                current = current->children[index];
            }

            if (!current->is_end_of_word) {
                current->is_end_of_word = true;
                current_size++;
            }
            current->value = value;
        }

        // Removes all elements and resets the dictionary
        void clear() {
            clear_node(root);
            root = new TrieNode();
            current_size = 0;
        }

        // Returns the number of elements currently stored
        size_t size() const {
            return current_size;
        }

        // Retrieves the value by key. Returns default Data() if not found.
        Data get(const std::string& inx) const {
            TrieNode* current = root;
            for (char c : inx) {
                int index = get_index(c);
                if (!current->children[index]) {
                    return Data();
                }
                current = current->children[index];
            }

            if (current->is_end_of_word) {
                return current->value;
            }
            return Data();
        }
};
#endif

/* ---------------------------------------------------------------------- */

#ifdef USE_CUSTOM_DICTIONARY
template <typename Data>
class Dictionary<std::string, Data> {
    private:
        struct HashEntry {
            std::string key;
            Data value;
            bool is_occupied;
            bool was_deleted; // Needed for linear probing deletion if expanded later

            HashEntry() : key(""), value(Data()), is_occupied(false), was_deleted(false) {}
        };

        HashEntry* table;
        size_t table_capacity;
        size_t current_size;
        const double MAX_LOAD_FACTOR = 0.7; // Resize when 70% full

        // FNV-1a Hash Function - Excellent for strings and 256 ASCII characters
        size_t hash_function(const std::string& key) const {
            size_t hash = 14695981039346656037ULL; // FNV offset basis
            for (char c : key) {
                // Safely handle 256 ASCII characters by casting to unsigned char
                hash ^= static_cast<unsigned char>(c);
                hash *= 1099511628211ULL; // FNV prime
            }
            return hash % table_capacity;
        }

        // Internal helper to resize and rehash the table
        void resize(size_t new_capacity) {
            HashEntry* old_table = table;
            size_t old_capacity = table_capacity;

            table_capacity = new_capacity;
            table = new HashEntry[table_capacity];
            current_size = 0;

            for (size_t i = 0; i < old_capacity; ++i) {
                if (old_table[i].is_occupied) {
                    store(old_table[i].key, old_table[i].value);
                }
            }
            delete[] old_table;
        }

    public:
        // Default Constructor - starts with a base capacity of 16
        Dictionary() : table_capacity(16), current_size(0) {
            table = new HashEntry[table_capacity];
        }

        // Destructor
        ~Dictionary() {
            delete[] table;
        }

        // Copy Constructor - Deep copy
        Dictionary(const Dictionary& other)
            : table_capacity(other.table_capacity), current_size(other.current_size) {
            table = new HashEntry[table_capacity];
            for (size_t i = 0; i < table_capacity; ++i) {
                table[i] = other.table[i];
            }
        }

        // Copy Assignment Operator
        Dictionary& operator=(const Dictionary& other) {
            if (this != &other) {
                HashEntry* new_table = new HashEntry[other.table_capacity];
                for (size_t i = 0; i < other.table_capacity; ++i) {
                    new_table[i] = other.table[i];
                }
                delete[] table;
                table = new_table;
                table_capacity = other.table_capacity;
                current_size = other.current_size;
            }
            return *this;
        }

        // Checks if the key exists in the table
        bool contains(const std::string& inx) const {
            size_t index = hash_function(inx);
            size_t start_index = index;

            while (table[index].is_occupied || table[index].was_deleted) {
                if (table[index].is_occupied && table[index].key == inx) {
                    return true;
                }
                index = (index + 1) % table_capacity;
                if (index == start_index) break; // Traversed the entire table
            }
            return false;
        }

        // Inserts or updates a value
        void store(const std::string& inx, const Data& value) {
            // Resize if the load factor threshold is crossed
            if (static_cast<double>(current_size + 1) / table_capacity > MAX_LOAD_FACTOR) {
                resize(table_capacity * 2);
            }

            size_t index = hash_function(inx);

            while (table[index].is_occupied) {
                // If key already exists, update its value
                if (table[index].key == inx) {
                    table[index].value = value;
                    return;
                }
                index = (index + 1) % table_capacity; // Linear probing
            }

            // Insert new element
            table[index].key = inx;
            table[index].value = value;
            table[index].is_occupied = true;
            table[index].was_deleted = false;
            current_size++;
        }

        // Resets the dictionary
        void clear() {
            delete[] table;
            table_capacity = 16;
            current_size = 0;
            table = new HashEntry[table_capacity];
        }

        // Returns the number of elements
        size_t size() const {
            return current_size;
        }

        // Retrieves a value by key. Returns default Data() if not found.
        Data get(const std::string& inx) const {
            size_t index = hash_function(inx);
            size_t start_index = index;

            while (table[index].is_occupied || table[index].was_deleted) {
                if (table[index].is_occupied && table[index].key == inx) {
                    return table[index].value;
                }
                index = (index + 1) % table_capacity;
                if (index == start_index) break;
            }
            return Data();
        }
};
#endif

/* ---------------------------------------------------------------------- */

#ifdef USE_CUSTOM_DICTIONARY
template <typename Data>
class Dictionary<long, Data> {
    private:
        struct HashEntry {
            long key;
            Data value;
            bool is_occupied;
            bool was_deleted;

            HashEntry() : key(0), value(Data()), is_occupied(false), was_deleted(false) {}
        };

        HashEntry* table;
        size_t table_capacity;
        size_t current_size;
        const double MAX_LOAD_FACTOR = 0.7; // Resize when 70% full

        // Golden Ratio multiplier for 64-bit Fibonacci Hashing (very fast and distributes well)
        size_t hash_function(long key) const {
            unsigned long ukey = static_cast<unsigned long>(key);
            return (ukey * 11400714819323198485ULL) % table_capacity;
        }

        // Internal helper to double the size of the table and rehash
        void resize(size_t new_capacity) {
            HashEntry* old_table = table;
            size_t old_capacity = table_capacity;

            table_capacity = new_capacity;
            table = new HashEntry[table_capacity];
            current_size = 0;

            for (size_t i = 0; i < old_capacity; ++i) {
                if (old_table[i].is_occupied) {
                    store(old_table[i].key, old_table[i].value);
                }
            }
            delete[] old_table;
        }

    public:
        // Default Constructor - starts with a base capacity of 16 (must be power of 2 for optimal sizing)
        Dictionary() : table_capacity(16), current_size(0) {
            table = new HashEntry[table_capacity];
        }

        // Destructor
        ~Dictionary() {
            delete[] table;
        }

        // Copy Constructor - Deep copy
        Dictionary(const Dictionary& other)
            : table_capacity(other.table_capacity), current_size(other.current_size) {
            table = new HashEntry[table_capacity];
            for (size_t i = 0; i < table_capacity; ++i) {
                table[i] = other.table[i];
            }
        }

        // Copy Assignment Operator
        Dictionary& operator=(const Dictionary& other) {
            if (this != &other) {
                HashEntry* new_table = new HashEntry[other.table_capacity];
                for (size_t i = 0; i < other.table_capacity; ++i) {
                    new_table[i] = other.table[i];
                }
                delete[] table;
                table = new_table;
                table_capacity = other.table_capacity;
                current_size = other.current_size;
            }
            return *this;
        }

        // Checks if the 64-bit key exists in the table
        bool contains(long inx) const {
            size_t index = hash_function(inx);
            size_t start_index = index;

            while (table[index].is_occupied || table[index].was_deleted) {
                if (table[index].is_occupied && table[index].key == inx) {
                    return true;
                }
                index = (index + 1) % table_capacity;
                if (index == start_index) break;
            }
            return false;
        }

        // Inserts or updates a value
        void store(long inx, const Data& value) {
            if (static_cast<double>(current_size + 1) / table_capacity > MAX_LOAD_FACTOR) {
                resize(table_capacity * 2);
            }

            size_t index = hash_function(inx);

            while (table[index].is_occupied) {
                if (table[index].key == inx) {
                    table[index].value = value;
                    return;
                }
                index = (index + 1) % table_capacity;
            }

            table[index].key = inx;
            table[index].value = value;
            table[index].is_occupied = true;
            table[index].was_deleted = false;
            current_size++;
        }

        // Resets the dictionary
        void clear() {
            delete[] table;
            table_capacity = 16;
            current_size = 0;
            table = new HashEntry[table_capacity];
        }

        // Returns the number of elements
        size_t size() const {
            return current_size;
        }

        // Retrieves a value. Returns default Data() if not found.
        Data get(long inx) const {
            size_t index = hash_function(inx);
            size_t start_index = index;

            while (table[index].is_occupied || table[index].was_deleted) {
                if (table[index].is_occupied && table[index].key == inx) {
                    return table[index].value;
                }
                index = (index + 1) % table_capacity;
                if (index == start_index) break;
            }
            return Data();
        }
};
#endif

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE

#endif /* __PROG_DICTIONARY_H__ */
