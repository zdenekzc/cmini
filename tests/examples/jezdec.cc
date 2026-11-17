#include <stdio.h>
#include <iostream>
using namespace std;

const int N = 8;
const int limit = 100;

int p[N][N];
int zx[N][N];
int zy[N][N];

void jump(int i, int k, int v, int i0, int k0)
// skoc na (i,k) a je to v-ty skok, skaceme z (i0, k0)
{
    if (i >= 0 && i < N && k >= 0 && k < N && v <= limit)
    {
        if (p[i][k] == -1 || p[i][k] > v)
        {
            p[i][k] = v;
            zx[i][k] = i0;
            zy[i][k] = k0;

            jump (i+2, k+1, v+1, i, k);
            jump (i+2, k-1, v+1, i, k);
            jump (i-2, k+1, v+1, i, k);
            jump (i-2, k-1, v+1, i, k);

            jump (i+1, k+2, v+1, i, k);
            jump (i+1, k-2, v+1, i, k);
            jump (i-1, k+2, v+1, i, k);
            jump (i-1, k-2, v+1, i, k);
        }
    }
}

void search (int i, int k)
{
    if (i >= 0 && k >= 0)
    {
        search (zx[i][k], zy[i][k]);
        cout << i << "," << k << endl;
    }
}

int main()
{
    for (int i = 0; i < N; i++)
        for (int k = 0; k < N; k++)
        {
            p[i][k] = -1;
            zx[i][k] = -1;
            zy[i][k] = -1;
        }

    jump (0, 0, 0, -1, -1);

    for (int i = 0; i < N; i++)
    {
        for (int k = 0; k < N; k++)
        {
            if (p[i][k] == -1)
                cout << ".";
            else
                cout << p[i][k];
            cout << " ";
        }
        cout << endl;
    }

    cout << "--------" << endl;

    search (N-1, N-1);

    cout << "O.K." << endl;
}
