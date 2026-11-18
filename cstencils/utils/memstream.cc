
/* memstream.cc */

#include "memstream.h"
#include "ioexception.h"
#include "inf.h" // function info

#include <string.h> // function strcmp

#include <fstream>

OPEN_NAMESPACE

/* ---------------------------------------------------------------------- */

void MemStream::open (string p_file_name)
{
   file_name = p_file_name;
}

string MemStream::get_file_name ()
{
   return file_name;
}

string MemStream::get_text ()
{
   return this->str();
}

void MemStream::enableOutputMessages (bool value)
{
   show_msg = value;
}

MemStream::MemStream () :
   file_name (),
   show_msg (true)
{ }

/* ---------------------------------------------------------------------- */

void MemStream::write_text ()
{
   ofstream f (file_name.c_str ());
   if (! f.good ())
      error ("Cannot open file for writing", file_name, 0, 0);

   f << this->str ();

   f.close ();
   if (! f.good ())
      error ("error writing file", file_name, 0, 0);
}

/* ---------------------------------------------------------------------- */

void MemStream::store ()
{
   this->flush ();

   bool equal = false;

   {
      ifstream f (file_name.c_str ());
      if (f.good ())
      {
         f.seekg (0, ifstream::end);

         streamsize size = f.tellg (); // MANDRAKE 9.0 (int)
         size_t size_unsigned = (size_t) size;

         string text = this->str();
         size_t text_length = text.length ();

         if (size >= 0 && size_unsigned == text_length)
         {
            char * data = new char [size+1];
            f.seekg (0, ifstream::beg);
            f.read (data, size);

            streamsize size2 = f.gcount ();
            if (size2 == size)
            {
                data [size] = 0;
                if (strcmp (text.c_str (), data) == 0)
                   equal = true;
            }

            delete [] data; // do not forget []
         }

         f.close ();
      }
   }

   if (! equal)
   {
      write_text ();
      if (show_msg)
         info ("writing " + file_name);
   }
   else
   {
      if (show_msg)
         info ("not modified " + file_name);
   }

}

void MemStream::store_always ()
{
   write_text ();
   if (show_msg)
      info ("unconditionally writing " + file_name);
}

/* ---------------------------------------------------------------------- */

bool MemStream::read_from (string p_file_name)
{
   ifstream f (p_file_name.c_str ());

   if (f.good ())
   {
      f.seekg (0, ifstream::end);
      size_t size = f.tellg ();

      if (size >= 0)
      {
         char * data = new char [size];
         f.seekg (0, ifstream::beg);
         f.read (data, size);

         this->write (data, size);
         delete [] data;
      }
   }

   return f.good ();
}

/* ---------------------------------------------------------------------- */

#if 0
class MemStream {
    protected:
        String file_name;
        bool show_msg;
        Sequence<char> buffer; 

        void write_text() {
            std::FILE* f = std::fopen(file_name.c_str(), "wb");
            if (!f) {
                error("Cannot open file for writing", file_name.c_str(), 0, 0);
                return;
            }

            if (buffer.size() > 0) {
                for (int i = 0; i < buffer.size(); ++i) {
                    std::fputc(buffer.get(i), f);
                }
            }

            std::fclose(f);
        }

    public:
        MemStream() : file_name(""), show_msg(true) {}

        virtual ~MemStream() {}

        void open(String p_file_name) {
            file_name = p_file_name;
        }

        void enableOutputMessages(bool value) {
            show_msg = value;
        }

        String get_file_name() const {
            return file_name;
        }

        MemStream& operator<<(const char* s) {
            if (!s) return *this;
            while (*s != '\0') {
                buffer.add(*s);
                s++;
            }
            return *this;
        }

        MemStream& operator<<(const String& s) {
            return *this << s.c_str();
        }

        MemStream& operator<<(char c) {
            buffer.add(c);
            return *this;
        }

        String get_text() const {
            int len = buffer.size();
            // Create a null-terminated temporary char array
            char* temp = new char[len + 1];
            for (int i = 0; i < len; ++i) {
                temp[i] = buffer.get(i);
            }
            temp[len] = '\0';

            String result(temp);
            delete[] temp;
            return result;
        }

        bool read_from(String source_file_name) {
            std::FILE* f = std::fopen(source_file_name.c_str(), "rb");
            if (!f) return false;

            // Determine file size
            std::fseek(f, 0, SEEK_END);
            long size = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);

            if (size > 0) {
                char* data = new char[size];
                size_t read_bytes = std::fread(data, 1, size, f);

                for (size_t i = 0; i < read_bytes; ++i) {
                    buffer.add(data[i]);
                }
                delete[] data;
            }

            std::fclose(f);
            return true;
        }

        void store() {
            bool equal = false;

            std::FILE* f = std::fopen(file_name.c_str(), "rb");
            if (f) {
                std::fseek(f, 0, SEEK_END);
                long file_size = std::ftell(f);
                std::fseek(f, 0, SEEK_SET);

                int current_buffer_size = buffer.size();

                if (file_size >= 0 && static_cast<size_t>(file_size) == static_cast<size_t>(current_buffer_size)) {
                    char* data = new char[file_size + 1];
                    size_t read_bytes = std::fread(data, 1, file_size, f);
                    data[read_bytes] = '\0';

                    if (static_cast<long>(read_bytes) == file_size) {
                        equal = true;
                        for (int i = 0; i < current_buffer_size; ++i) {
                            if (buffer.get(i) != data[i]) {
                                equal = false;
                                break;
                            }
                        }
                    }
                    delete[] data;
                }
                std::fclose(f);
            }

            if (!equal) {
                write_text();
                if (show_msg) {
                    info(String("writing ") + file_name);
                }
            } else {
                if (show_msg) {
                    info(String("not modified ") + file_name);
                }
            }
        }

        void store_always() {
            write_text();
            if (show_msg) {
                info(String("unconditionally writing ") + file_name);
            }
        }
};
#endif

/* ---------------------------------------------------------------------- */

CLOSE_NAMESPACE
