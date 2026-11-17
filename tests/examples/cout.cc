#include <iostream>
using namespace std;
/* cout / cerr (cmini => printf / dprintf; vzor: g++ s iostream) */
int main ()
{
   string a = "ab";
   string b = "cd";
   int x = 3;
   int r = 1 << 3;
   bool t = x > 2;
   char buf [8] = "pole";
   unsigned long big = 18446744073709551615ul;
   short sh = -5;
   cout << a + b << std::endl;
   cout << "bool " << t << " " << (x > 5) << " r=" << r << endl;
   cout << "shift " << (1 << 4) << " " << (x << 2) << endl;
   cout << buf << " " << big << " " << sh << endl;
   cout << 2.0 / 3 << " " << 1e6 << " " << 1e-5 << endl;
   cerr << "chyba: " << x + 1 << endl;
   cout << endl;
   cout << "konec";
   cout << endl;
   return 0;
}
