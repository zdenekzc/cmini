
/* sequence.h */

#ifndef __PROG_SEQUENCE_H__
#define __PROG_SEQUENCE_H__

#include "std.h"
#include <vector>

OPEN_NAMESPACE

template <typename Data>
class Sequence
{
   private:
      vector <Data> list;

   public:
      // Data operator [] (int inx) { return list [inx]; }
      Data get (int inx) { return list [inx]; }

      bool empty () { return list.empty (); }
      int  size ()  { return list.size (); }

      void add (Data value)         { list.push_back (value); }
      void insertFirst (Data value) { list.insert (list.begin (), value); }

      void push (Data  value) { list.push_back (value); }
      void pop ()             { list.resize (list.size () - 1); }
      Data top ()             { return list [list.size () - 1]; }

      Data last ()                 { return list [list.size()-1]; }
      void changeLast (Data value) { list [list.size()-1] = value; }

      void change (int inx, Data value) { list [inx] = value; }

      void clear ()
      {
         // while (! list.empty ())
         //   pop ();
          list.resize (0);
      }

      bool contains (Data value)
      {
          int cnt = list.size ();
          int inx = 0;
          while (inx < cnt && list [inx] != value) inx ++;
          return inx < cnt;
      }

      // http://stackoverflow.com/questions/8164567/how-to-make-my-custom-type-to-work-with-range-based-for-loops
      // #if __cplusplus >= 201103L // C++11
      //    auto begin() { return list.begin(); }
      //    auto end() { return list.end(); }
      // #endif

      // auto cbegin() const { return list.begin(); }
      // auto cend() const { return list.end(); }
      // auto begin() const { return list.begin(); }
      // auto end() const { return list.end(); }
};

/* ---------------------------------------------------------------------- */

#ifdef USE_CUSTOM_SEQUENCE
template <typename Data>
class Sequence {
    private:
        Data* array;          // Raw C-array to hold elements
        int current_capacity; // Total allocated slots
        int current_size;     // Number of elements currently stored

        // Internal helper to resize the underlying C-array
        void reserve(int new_capacity) {
            if (new_capacity < current_size) return;

            Data* new_array = new Data[new_capacity];
            for (int i = 0; i < current_size; ++i) {
                new_array[i] = array[i]; // Copy existing elements
            }

            delete[] array; // Free old memory
            array = new_array;
            current_capacity = new_capacity;
        }

    public:
        // Default Constructor
        Sequence() : array(new Data[4]), current_capacity(4), current_size(0) {}

        // Destructor - clean up dynamically allocated C-array
        ~Sequence() {
            delete[] array;
        }

        // Copy Constructor - Deep copy
        Sequence(const Sequence& other)
            : current_capacity(other.current_capacity), current_size(other.current_size) {
            array = new Data[current_capacity];
            for (int i = 0; i < current_size; ++i) {
                array[i] = other.array[i];
            }
        }

        // Copy Assignment Operator
        Sequence& operator=(const Sequence& other) {
            if (this != &other) {
                Data* new_array = new Data[other.current_capacity];
                for (int i = 0; i < other.current_size; ++i) {
                    new_array[i] = other.array[i];
                }
                delete[] array;
                array = new_array;
                current_capacity = other.current_capacity;
                current_size = other.current_size;
            }
            return *this;
        }

        // Returns element at specific index
        Data get(int inx) const {
            return array[inx];
        }

        // Checks if the sequence is empty
        bool empty() const {
            return current_size == 0;
        }

        // Returns current number of elements
        int size() const {
            return current_size;
        }

        // Adds element to the end (std::vector::push_back)
        void add(Data value) {
            if (current_size >= current_capacity) {
                reserve(current_capacity * 2);
            }
            array[current_size] = value;
            current_size++;
        }

        // Inserts an element at the very beginning (shifts all other elements)
        void insertFirst(Data value) {
            if (current_size >= current_capacity) {
                reserve(current_capacity * 2);
            }
            // Shift elements to the right to make room at index 0
            for (int i = current_size; i > 0; --i) {
                array[i] = array[i - 1];
            }
            array[0] = value;
            current_size++;
        }

        // Stack interface aliases
        void push(Data value) {
            add(value);
        }

        void pop() {
            if (current_size > 0) {
                current_size--;
            }
        }

        Data top() const {
            return array[current_size - 1];
        }

        // Returns the last element
        Data last() const {
            return array[current_size - 1];
        }

        // Modifies the last element
        void changeLast(Data value) {
            array[current_size - 1] = value;
        }

        // Modifies element at specific index
        void change(int inx, Data value) {
            array[inx] = value;
        }

        // Clears the sequence by resetting the size counter
        void clear() {
            current_size = 0;
        }

        // Linear search to check if value exists
        bool contains(Data value) const {
            int inx = 0;
            while (inx < current_size && array[inx] != value) {
                inx++;
            }
            return inx < current_size;
        }
};
#endif

/* ---------------------------------------------------------------------- */

typedef Sequence <string> StringSequence;

CLOSE_NAMESPACE

#endif /* __PROG_QUEUE_H__ */
