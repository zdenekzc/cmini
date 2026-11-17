/* shapes.h - spolecna hlavicka programu z vice souboru (tests/examples/multi) */

#ifndef SHAPES_H
#define SHAPES_H

#include <string>
#include "../../../cmini/ownheaders/containers.h"
using namespace std;

class Shape
{
   public:
      string name;
      Shape ();
      virtual ~Shape ();
      virtual double area () = 0;
      string describe () { return name + " " + kind (); }   /* telo ve tride - v kazdem souboru */
      virtual string kind () { return "shape"; }
};

class Circle : public Shape
{
   public:
      double r;
      Circle (double radius);
      ~Circle ();
      double area ();
      string kind ();
};

class Square : public Shape
{
   public:
      double a;
      Square (double side);
      double area ();
};

class Registry                      /* globalni objekt s konstruktorem a destruktorem */
{
   public:
      int created;
      Registry ();
      ~Registry ();
};

extern int shape_count;             /* definice v shapes.c */
extern Sequence <int> history;
extern Registry registry;

Shape * make_shape (int k);
double total_area (Shape * * a, int n);
int counter_value ();
extern "C" int cp_callback (int x); /* vola ji util.c (C) */

extern "C" int c_twice (int x);     /* util.c, prelozeno gcc */
extern "C" int c_apply (int x);

#endif
